#include <LibraryLinkUtilities/Macros.h>

import LibraryLinkUtilities.Base;
import LibraryLinkUtilities.MArgumentQueue;
import LibraryLinkUtilities.TimeSeries;
import System.Base;
import System.JSON;
import System.Math;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;
import MultiviewABRSimulation.MultiviewABRSimulator;
import MultiviewABRSimulation.ThroughputPredictors.EMAPredictor;
import MultiviewABRSimulation.ThroughputPredictors.IThroughputPredictor;
import MultiviewABRSimulation.ThroughputPredictors.MovingAveragePredictor;
import MultiviewABRSimulation.ViewPredictors.IViewPredictor;
import MultiviewABRSimulation.ViewPredictors.MarkovPredictor;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;
using namespace experimental;

LLU_GENERATE_TIME_SERIES_VIEW_GETTER(double)

LLU_GENERATE_TIME_SERIES_VIEW_GETTER(int64_t)

LLU_GENERATE_ABSTRACT_STRUCT_GETTER(BaseThroughputPredictorOptions, (
                                        EMAPredictorOptions,
                                        MovingAveragePredictorOptions
                                    ))

LLU_GENERATE_ABSTRACT_STRUCT_GETTER(BaseViewPredictorOptions, (
                                        MarkovPredictorOptions,
                                        StaticPredictorOptions
                                    ))

LLU_GENERATE_ABSTRACT_STRUCT_GETTER(BaseMultiviewABRControllerOptions, (
                                        ThroughputBasedControllerOptions
                                    ))

extern "C" __declspec(dllexport)
int WolframLibrary_initialize(WolframLibraryData libraryData) {
    return LLU::TryInvoke([&] {
        LLU::LibraryData::setLibraryData(libraryData);
        LLU::ErrorManager::registerPacletErrors(LLU::PacletErrors);
    });
}

/// Simulates a multiview adaptive bitrate streaming configuration on a collection of network series and primary stream series.
/// @param streamingConfig ["Object"] The adaptive bitrate streaming configuration.
/// @param controllerOptions ["TypedOptions"] The options for the multiview adaptive bitrate controller.
/// @param networkData [LibraryDataType[TemporalData, Real]] A collection of network series.
/// @param primaryStreamData [LibraryDataType[TemporalData, Integer]] A collection of primary stream series.
/// @param throughputPredictorOptions ["TypedOptions"] The options for the throughput predictor.
/// @param viewPredictorOptions ["TypedOptions"] The options for the view predictor.
/// @returns ["DataStore"] A collection of simulation series.
extern "C" __declspec(dllexport)
int MultiviewABRSimulate(WolframLibraryData, int64_t argc, MArgument *args, MArgument out) {
    return LLU::TryInvoke([&] {
        LLU::MArgumentQueue argQueue(argc, args, out);
        const auto streamingConfig = argQueue.Pop<StreamingConfig>();
        const auto controllerOptions = argQueue.Pop<unique_ptr<BaseMultiviewABRControllerOptions>>();
        const auto networkData = argQueue.Pop<NetworkDataView>();
        const auto primaryStreamData = argQueue.Pop<PrimaryStreamDataView>();
        const auto throughputPredictorOptions = argQueue.Pop<unique_ptr<BaseThroughputPredictorOptions>>();
        const auto viewPredictorOptions = argQueue.Pop<unique_ptr<BaseViewPredictorOptions>>();

        const auto sessionCount = primaryStreamData.PathCount();
        const auto groupCount = Math::Round(primaryStreamData.DurationSeconds() / streamingConfig.SegmentSeconds);
        const auto streamCount = streamingConfig.StreamCount;
        LLU::Tensor rebufferingSeconds(0., {sessionCount});
        LLU::Tensor bufferedBitratesMbps(0., {sessionCount, groupCount, streamCount});
        LLU::Tensor primaryStreamDistributions(0., {sessionCount, groupCount, streamCount});
        LLU::Tensor downloadedMB(0., {sessionCount}), rawWastedMB(0., {sessionCount});
        const SimulationDataRef simulationData = {
            rebufferingSeconds, LLU::ToMDSpan<double, dims<3>>(bufferedBitratesMbps),
            LLU::ToMDSpan<double, dims<3>>(primaryStreamDistributions), downloadedMB, rawWastedMB
        };
        MultiviewABRSimulator::Simulate(streamingConfig, *controllerOptions,
                                        networkData, primaryStreamData, simulationData,
                                        {*throughputPredictorOptions, *viewPredictorOptions});

        LLU::DataList<LLU::NodeType::Any> _out;
        _out.push_back("RebufferingSeconds", move(rebufferingSeconds));
        _out.push_back("BufferedBitratesMbps", move(bufferedBitratesMbps));
        _out.push_back("PrimaryStreamDistributions", move(primaryStreamDistributions));
        _out.push_back("DownloadedMB", move(downloadedMB));
        _out.push_back("RawWastedMB", move(rawWastedMB));
        argQueue.SetOutput(_out);
    });
}
