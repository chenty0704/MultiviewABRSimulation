module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;

import System.Base;
import System.Math;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.ViewPredictors.IViewPredictor;

using namespace std;
using namespace experimental;

/// Represents the base options for a multiview adaptive bitrate controller.
export struct BaseMultiviewABRControllerOptions {
    double ThroughputDiscount = 1.; ///< The discount factor for predicted throughputs.

    virtual ~BaseMultiviewABRControllerOptions() = default;
};

export {
    DESCRIBE_STRUCT(BaseMultiviewABRControllerOptions, (), (
                        ThroughputDiscount
                    ))
}

/// Provides context for a multiview adaptive bitrate controller.
export struct MultiviewABRControllerContext {
    double ThroughputMbps; ///< The predicted throughput in megabits per second.
    double BufferSeconds; ///< The buffer level in seconds.
    mdspan<const int, dims<2>> BufferedBitrateIDs; ///< The bitrate IDs of buffered segments.
    const IViewPredictor &ViewPredictor; ///< The view predictor.
};

/// Represents a control action.
export struct ControlAction {
    int GroupID; ///< The index of the segment group to download or upgrade.
    vector<int> BitrateIDs; ///< The bitrate IDs for the segment group.
};

/// Defines the interface of a multiview adaptive bitrate controller.
export class IMultiviewABRController {
public:
    virtual ~IMultiviewABRController() = default;

    /// Gets the control action given the specified context.
    /// @param context The context for the multiview adaptive bitrate controller.
    /// @returns The control action given the specified context.
    [[nodiscard]] virtual ControlAction GetControlAction(const MultiviewABRControllerContext &context) const = 0;
};

/// Provides a skeletal implementation of a multiview adaptive bitrate controller.
export class BaseMultiviewABRController : public IMultiviewABRController {
protected:
    double _segmentSeconds;
    span<const double> _bitratesMbps;
    int _streamCount;
    double _primaryViewSize, _secondaryViewSize;
    double _maxBufferSeconds;
    double _throughputDiscount;

    vector<double> _weightedPrimaryUtilities, _weightedSecondaryUtilities;

    explicit BaseMultiviewABRController(const StreamingConfig &streamingConfig,
                                        const BaseMultiviewABRControllerOptions &options = {}) :
        _segmentSeconds(streamingConfig.SegmentSeconds), _bitratesMbps(streamingConfig.BitratesMbps),
        _streamCount(streamingConfig.StreamCount), _primaryViewSize(streamingConfig.PrimaryViewSize),
        _secondaryViewSize((1 - _primaryViewSize) / (_streamCount - 1)),
        _maxBufferSeconds(streamingConfig.MaxBufferSeconds), _throughputDiscount(options.ThroughputDiscount) {
        const auto minBitrateMbps = _bitratesMbps.front(), maxBitrateMbps = _bitratesMbps.back();
        const auto utilityNormalizer = Math::Log(maxBitrateMbps / minBitrateMbps);
        const auto PrimaryUtility =
            [&](double bitrateMbps) { return Math::Log(bitrateMbps / minBitrateMbps) / utilityNormalizer; };

        _weightedPrimaryUtilities = _bitratesMbps | views::transform([&](double bitrateMbps) {
            return _primaryViewSize * PrimaryUtility(bitrateMbps);
        }) | ranges::to<vector>();
        _weightedSecondaryUtilities = _bitratesMbps | views::transform([&](double bitrateMbps) {
            const auto scaledBitrateMbps = min(_primaryViewSize * bitrateMbps / _secondaryViewSize, maxBitrateMbps);
            return _secondaryViewSize * PrimaryUtility(scaledBitrateMbps);
        }) | ranges::to<vector>();
    }
};
