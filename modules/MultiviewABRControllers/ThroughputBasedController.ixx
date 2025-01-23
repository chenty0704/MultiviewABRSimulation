module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;

import System.Base;

import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;

using namespace std;

/// Represents the options for a throughput-based controller.
export struct ThroughputBasedControllerOptions : BaseMultiviewABRControllerOptions {
};

export {
    DESCRIBE_STRUCT(ThroughputBasedControllerOptions, (BaseMultiviewABRControllerOptions), ())
}

/// A throughput-based controller makes bitrate decisions by optimizing the multiview utility subject to throughput constraint.
export class ThroughputBasedController : public BaseMultiviewABRController {
public:
    /// Creates a throughput-based controller with the specified configuration and options.
    /// @param config The configuration for the throughput-based controller.
    /// @param options The options for the throughput-based controller.
    explicit ThroughputBasedController(const MultiviewABRControllerConfig &config,
                                       const ThroughputBasedControllerOptions &options = {}) :
        BaseMultiviewABRController(config, options) {
    }

    [[nodiscard]] ControlAction GetControlAction(const MultiviewABRControllerContext &context) const override {
        const auto throughputMbps = context.ThroughputMbps * _throughputDiscount;
        const auto distribution = _viewPredictor.get().PredictPrimaryStreamDistribution(context.BufferSeconds);

        vector<int> bitrateIDs(_streamCount);
        auto totalBitrateMbps = _bitratesMbps.front() * _streamCount;
        auto derivatives = views::iota(0, _streamCount) | views::transform([&](int streamID) {
            return GetDerivative(bitrateIDs[streamID], distribution[streamID]);
        }) | ranges::to<vector>();
        while (true) {
            const auto streamID = static_cast<int>(distance(derivatives.begin(), ranges::max_element(derivatives)));
            if (bitrateIDs[streamID] == _bitratesMbps.size() - 1) break;

            totalBitrateMbps += _bitratesMbps[bitrateIDs[streamID] + 1] - _bitratesMbps[bitrateIDs[streamID]];
            if (totalBitrateMbps > throughputMbps) break;

            derivatives[streamID] = GetDerivative(++bitrateIDs[streamID], distribution[streamID]);
        }
        return {static_cast<int>(context.BufferedBitrateIDs.extent(0)), bitrateIDs};
    }

private:
    [[nodiscard]] double GetDerivative(int bitrateID, double probability) const {
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
