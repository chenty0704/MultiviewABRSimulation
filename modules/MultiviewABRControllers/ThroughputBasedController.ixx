module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;

import System.Base;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;

using namespace std;

/// Represents the options for a throughput-based controller.
export struct ThroughputBasedControllerOptions : BaseMultiviewABRControllerOptions {
};

export {
    DESCRIBE_STRUCT(ThroughputBasedControllerOptions, (BaseMultiviewABRControllerOptions), ())
}

/// A throughput-based controller decides bitrates by optimizing the multiview utility subject to throughput constraint.
export class ThroughputBasedController : public BaseMultiviewABRController {
public:
    /// Creates a throughput-based controller with the specified configuration and options.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param options The options for the throughput-based controller.
    explicit ThroughputBasedController(const StreamingConfig &streamingConfig,
                                       const ThroughputBasedControllerOptions &options = {}) :
        BaseMultiviewABRController(streamingConfig, options) {
    }

    [[nodiscard]] ControlAction GetControlAction(const MultiviewABRControllerContext &context) override {
        const auto throughputMbps = context.ThroughputMbps * _throughputDiscount;
        const auto bufferedGroupCount = static_cast<int>(context.BufferedBitrateIDs.extent(0));
        const auto distribution = context.ViewPredictor.PredictPrimaryStreamDistribution(
            context.BufferSeconds, _segmentSeconds);

        vector bitrateIDs(_streamCount, 0);
        auto totalBitrateMbps = _bitratesMbps.front() * _streamCount;
        auto derivatives = views::iota(0, _streamCount) | views::transform([&](int streamID) {
            return ExpectedUtilityDerivative(bitrateIDs[streamID], distribution[streamID]);
        }) | ranges::to<vector>();
        while (true) {
            const auto streamID = static_cast<int>(ranges::max_element(derivatives) - derivatives.cbegin());
            if (derivatives[streamID] == 0.) break;

            totalBitrateMbps += _bitratesMbps[bitrateIDs[streamID] + 1] - _bitratesMbps[bitrateIDs[streamID]];
            if (totalBitrateMbps > throughputMbps) break;

            derivatives[streamID] = ExpectedUtilityDerivative(++bitrateIDs[streamID], distribution[streamID]);
        }
        return {bufferedGroupCount, bitrateIDs};
    }

private:
    [[nodiscard]] double ExpectedUtilityDerivative(int bitrateID, double probability) const {
        if (bitrateID == _bitratesMbps.size() - 1) return 0.;

        const auto weightedPrimaryUtilityDiff =
            _weightedPrimaryUtilities[bitrateID + 1] - _weightedPrimaryUtilities[bitrateID];
        const auto weightedSecondaryUtilityDiff =
            _weightedSecondaryUtilities[bitrateID + 1] - _weightedSecondaryUtilities[bitrateID];
        const auto expectedUtilityDiff =
            probability * weightedPrimaryUtilityDiff + (1 - probability) * weightedSecondaryUtilityDiff;
        const auto bitrateDiffMbps = _bitratesMbps[bitrateID + 1] - _bitratesMbps[bitrateID];
        return expectedUtilityDiff / bitrateDiffMbps;
    }
};
