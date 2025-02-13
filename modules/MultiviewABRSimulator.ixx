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
    mdspan<double, dims<2>> BufferedBitratesMbps; ///< The 2D array of buffered bitrates in megabits per second.
    mdspan<double, dims<2>> PrimaryStreamDistributions; ///< The list of primary stream distributions.
    double &DownloadedMB; ///< The total downloaded size in megabytes.
    double &RawWastedMB; ///< The total wasted size in megabytes due to download abandonment and segment replacement.
};

/// Refers to a collection of simulation series.
export struct SimulationDataRef {
    span<double> RebufferingSeconds; ///< The list of total rebuffering durations in seconds.
    mdspan<double, dims<3>> BufferedBitratesMbps; ///< The 3D array of buffered bitrates in megabits per second.
    mdspan<double, dims<3>> PrimaryStreamDistributions; ///< The 2D array of primary stream distributions.
    span<double> DownloadedMB; ///< The list of total downloaded sizes in megabytes.
    span<double> RawWastedMB; ///< The list of total wasted sizes in megabytes.

    /// Returns the path at the specified index.
    /// @param index The index of the path.
    /// @returns The path at the specified index.
    [[nodiscard]] SimulationSeriesRef operator[](int index) const {
        const auto bitratesMbps = submdspan(BufferedBitratesMbps, index, full_extent, full_extent);
        const auto primaryStreamDistributions = submdspan(PrimaryStreamDistributions, index, full_extent, full_extent);
        return {
            RebufferingSeconds[index], bitratesMbps, primaryStreamDistributions, DownloadedMB[index], RawWastedMB[index]
        };
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

        const auto throughputPredictor = ThroughputPredictorFactory::Create(options.ThroughputPredictorOptions);
        const auto viewPredictor = ViewPredictorFactory::Create(
            streamCount, primaryStreamSeries.IntervalSeconds, options.ViewPredictorOptions);
        const auto controller = MultiviewABRControllerFactory::Create(streamingConfig, controllerOptions);
        NetworkSimulator networkSimulator(networkSeries);

        auto beginGroupID = 0, endGroupID = 0;
        auto secondsInGroup = 0.;
        mdarray<int, dims<2>> bufferedBitrateIDs(groupCount, streamCount);
        ranges::fill(bufferedBitrateIDs.container(), -1);
        out.RebufferingSeconds = 0.;
        out.DownloadedMB = 0., out.RawWastedMB = 0.;

        // Computes the primary stream distributions.
        const auto intervalCountPerSegment = Math::Round(segmentSeconds / primaryStreamSeries.IntervalSeconds);
        for (auto groupID = 0; groupID < groupCount; ++groupID) {
            const auto primaryStreamIDs = primaryStreamSeries.Window(groupID * segmentSeconds, segmentSeconds).Values;
            for (auto primaryStreamID : primaryStreamIDs)
                ++out.PrimaryStreamDistributions[groupID, primaryStreamID];
            for (auto streamID = 0; streamID < streamCount; ++streamID)
                out.PrimaryStreamDistributions[groupID, streamID] /= intervalCountPerSegment;
        }

        const auto ApplyControlAction = [&](int groupID, span<const int> bitrateIDs) {
            const span _bufferedBitrateIDs(&bufferedBitrateIDs[groupID, 0], streamCount);
            const auto totalSizeMB = *ranges::fold_left_first(
                views::iota(0, streamCount) | views::transform([&](int streamID) {
                    const auto bitrateID = bitrateIDs[streamID];
                    return bitrateID > _bufferedBitrateIDs[streamID] ? bitratesMbps[bitrateID] : 0.;
                }), plus()) * segmentSeconds / 8;
            const auto timeoutSeconds = groupID == endGroupID
                                            ? numeric_limits<double>::infinity()
                                            : (groupID - beginGroupID) * segmentSeconds - secondsInGroup;
            const auto [downloadedMB, downloadSeconds] = networkSimulator.Download(totalSizeMB, timeoutSeconds);
            throughputPredictor->Update(downloadedMB, downloadSeconds);

            out.DownloadedMB += downloadedMB;
            if (downloadedMB == totalSizeMB) {
                if (groupID < endGroupID)
                    out.RawWastedMB += *ranges::fold_left_first(
                        views::iota(0, streamCount) | views::transform([&](int streamID) {
                            const auto bufferedBitrateID = _bufferedBitrateIDs[streamID];
                            return bufferedBitrateID < bitrateIDs[streamID] ? bitratesMbps[bufferedBitrateID] : 0.;
                        }), plus()) * segmentSeconds / 8;
                ranges::copy(bitrateIDs, _bufferedBitrateIDs.begin());
            } else out.RawWastedMB += downloadedMB;
            if (groupID == endGroupID) ++endGroupID;
            return downloadSeconds;
        };

        const auto PlayVideo = [&](double seconds) {
            const auto currentSeconds = beginGroupID * segmentSeconds + secondsInGroup;
            const auto primaryStreamIDs = primaryStreamSeries.Window(currentSeconds, seconds).Values;
            if (!primaryStreamIDs.empty()) viewPredictor->Update(primaryStreamIDs);

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
            else PlayVideo(bufferSeconds), out.RebufferingSeconds += downloadSeconds - bufferSeconds;
        }

        // Plays the remaining buffer content.
        const auto bufferSeconds = (endGroupID - beginGroupID) * segmentSeconds - secondsInGroup;
        PlayVideo(bufferSeconds);

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
