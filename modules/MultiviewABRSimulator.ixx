export module MultiviewABRSimulation.MultiviewABRSimulator;

import System.Base;
import System.Math;
import System.MDArray;
import System.Parallel;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.MultiviewABRControllerFactory;
import MultiviewABRSimulation.NetworkSimulator;
import MultiviewABRSimulation.ThroughputPredictors.EMAPredictor;
import MultiviewABRSimulation.ThroughputPredictors.IThroughputPredictor;
import MultiviewABRSimulation.ThroughputPredictors.ThroughputPredictorFactory;
import MultiviewABRSimulation.ViewPredictors.IViewPredictor;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;
import MultiviewABRSimulation.ViewPredictors.ViewPredictorFactory;

using namespace std;
using namespace experimental;

/// Refers to a simulation series.
export struct SimulationSeriesRef {
    double &RebufferingSeconds; ///< The total rebuffering duration in seconds.
    mdspan<double, dims<2>> BufferedBitratesMbps; ///< A 2D array of buffered bitrates in megabits per second.
};

/// Refers to a collection of simulation series.
export struct SimulationDataRef {
    span<double> RebufferingSeconds; ///< A list of total rebuffering durations in seconds.
    mdspan<double, dims<3>> BufferedBitratesMbps; ///< A 3D array of buffered bitrates in megabits per second.

    /// Returns the path at the specified index.
    /// @param index The index of the path.
    /// @returns The path at the specified index.
    [[nodiscard]] SimulationSeriesRef operator[](int index) const {
        const auto bitratesMBps = submdspan(BufferedBitratesMbps, index, full_extent, full_extent);
        return {RebufferingSeconds[index], bitratesMBps};
    }
};

/// Represents the options for multiview adaptive bitrate streaming simulation.
export struct MultiviewABRSimulationOptions {
    /// The options for the throughput predictor.
    const BaseThroughputPredictorOptions &ThroughputPredictorOptions = EMAPredictorOptions();
    /// The options for the view predictor.
    const BaseViewPredictorOptions &ViewPredictorOptions = StaticPredictorOptions();
};

/// Simulates the dynamics of multiview adaptive bitrate streaming.
export class MultiviewABRSimulator {
public:
    /// Simulates a multiview adaptive bitrate streaming configuration on a network series and primary stream series.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param controllerOptions The options for the multiview adaptive bitrate controller.
    /// @param networkSeries A network series.
    /// @param primaryStreamSeries A primary stream series.
    /// @param out The simulation series output.
    /// @param options The options for the multiview adaptive bitrate simulator.
    static void Simulate(const StreamingConfig &streamingConfig,
                         const BaseMultiviewABRControllerOptions &controllerOptions,
                         NetworkSeriesView networkSeries,
                         PrimaryStreamSeriesView primaryStreamSeries,
                         SimulationSeriesRef out,
                         const MultiviewABRSimulationOptions &options = {}) {
        const auto segmentSeconds = streamingConfig.SegmentSeconds;
        const auto bitratesMbps = streamingConfig.BitratesMbps;
        const auto groupCount = Math::Round(primaryStreamSeries.DurationSeconds() / segmentSeconds);
        const auto streamCount = streamingConfig.StreamCount;
        const auto maxBufferSeconds = streamingConfig.MaxBufferSeconds;
        const auto unitSeconds = primaryStreamSeries.IntervalSeconds;

        const auto throughputPredictor = ThroughputPredictorFactory::Create(options.ThroughputPredictorOptions);
        const auto viewPredictor = ViewPredictorFactory::Create(streamCount, unitSeconds, options.ViewPredictorOptions);
        const auto controller = MultiviewABRControllerFactory::Create(streamingConfig, controllerOptions);
        NetworkSimulator networkSimulator(networkSeries, {unitSeconds});

        auto beginGroupID = 0, endGroupID = 0;
        auto secondsInGroup = 0.;
        auto rebufferingSeconds = 0.;
        mdarray<int, dims<2>> bufferedBitrateIDs(groupCount, streamCount);
        ranges::fill(bufferedBitrateIDs.container(), -1);

        const auto ApplyControlAction = [&](int groupID, span<const int> bitrateIDs) {
            const span _bufferedBitrateIDs(&bufferedBitrateIDs[groupID, 0], streamCount);
            const auto totalSizeMB = Math::Sum(0, streamCount, [&](int streamID) {
                return bitrateIDs[streamID] > _bufferedBitrateIDs[streamID] ? bitratesMbps[bitrateIDs[streamID]] : 0.;
            }) * segmentSeconds / 8;
            const auto timeoutSeconds = groupID == endGroupID
                                            ? numeric_limits<double>::infinity()
                                            : (groupID - beginGroupID) * segmentSeconds - secondsInGroup;
            const auto [downloadedMB, downloadSeconds] = networkSimulator.Download(totalSizeMB, timeoutSeconds);
            throughputPredictor->Update(downloadedMB, downloadSeconds);

            if (downloadedMB == totalSizeMB) ranges::copy(bitrateIDs, _bufferedBitrateIDs.begin());
            if (groupID == endGroupID) ++endGroupID;
            return downloadSeconds;
        };

        const auto PlayVideo = [&](double seconds) {
            const auto currentSeconds = beginGroupID * segmentSeconds + secondsInGroup;
            viewPredictor->Update(primaryStreamSeries.Window(currentSeconds, seconds).Values);

            beginGroupID += Math::Floor(seconds / segmentSeconds);
            secondsInGroup += fmod(seconds, segmentSeconds);
            if (secondsInGroup >= segmentSeconds) ++beginGroupID, secondsInGroup -= segmentSeconds;
        };

        // Downloads the first segment group at the lowest bitrates.
        ApplyControlAction(0, vector(streamCount, 0));

        // Downloads the remaining segment groups.
        while (beginGroupID < groupCount) {
            const auto bufferSeconds = (endGroupID - beginGroupID) * segmentSeconds - secondsInGroup;
            const auto _bufferedBitrateIDs = submdspan(bufferedBitrateIDs.to_mdspan(),
                                                       pair(beginGroupID + 1, endGroupID), full_extent);
            const MultiviewABRControllerContext context =
                {throughputPredictor->PredictThroughputMbps(), bufferSeconds, _bufferedBitrateIDs, *viewPredictor};
            const auto action = controller->GetControlAction(context);
            const auto groupID = beginGroupID + action.GroupID + static_cast<int>(endGroupID > beginGroupID);

            if (groupID == endGroupID) {
                if (groupID == groupCount) break; // There are no more segment groups to download.
                if (bufferSeconds > maxBufferSeconds - segmentSeconds) { // The buffer is too full.
                    const auto idleSeconds = bufferSeconds + segmentSeconds - maxBufferSeconds;
                    networkSimulator.WaitFor(idleSeconds);
                    PlayVideo(idleSeconds);
                    continue;
                }
            }

            const auto downloadSeconds = ApplyControlAction(groupID, action.BitrateIDs);
            if (downloadSeconds <= bufferSeconds) PlayVideo(downloadSeconds);
            else PlayVideo(bufferSeconds), rebufferingSeconds += downloadSeconds - bufferSeconds;
        }

        // Plays the remaining buffer content.
        const auto bufferSeconds = (endGroupID - beginGroupID) * segmentSeconds - secondsInGroup;
        PlayVideo(bufferSeconds);

        out.RebufferingSeconds = rebufferingSeconds;
        ranges::transform(bufferedBitrateIDs.container(), out.BufferedBitratesMbps.data_handle(),
                          [&](int bitrateID) { return bitratesMbps[bitrateID]; });
    }

    /// Simulates a multiview adaptive bitrate streaming configuration on a collection of network series and primary stream series.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param controllerOptions The options for the multiview adaptive bitrate controller.
    /// @param networkData A collection of network series.
    /// @param primaryStreamData A collection of primary stream series.
    /// @param out The simulation data output.
    /// @param options The options for the multiview adaptive bitrate simulator.
    static void Simulate(const StreamingConfig &streamingConfig,
                         const BaseMultiviewABRControllerOptions &controllerOptions,
                         NetworkDataView networkData,
                         PrimaryStreamDataView primaryStreamData,
                         SimulationDataRef out,
                         const MultiviewABRSimulationOptions &options = {}) {
        Parallel::For(0, primaryStreamData.PathCount(), [&](int i) {
            Simulate(streamingConfig, controllerOptions, networkData[i], primaryStreamData[i], out[i], options);
        });
    }
};
