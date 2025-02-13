module;

#include <System/Macros.h>

export module MultiviewABRSimulation.MultiviewABRControllers.ModelPredictiveController;

import System.Base;
import System.Math;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;

using namespace std;
using namespace experimental;

/// Represents the options for a model predictive controller.
export struct ModelPredictiveControllerOptions : BaseMultiviewABRControllerOptions {
    int WindowLength = 4; ///< The number of segment groups in the optimization window.
    double TargetBufferRatio = 1.; ///< The target buffer level (normalized by the maximum buffer level).
    double BufferCostWeight = 1.; ///< The weight of buffer cost.
    bool AllowUpgrades = true; ///< Whether the allow segment upgrades.
};

export {
    DESCRIBE_STRUCT(ModelPredictiveControllerOptions, (BaseMultiviewABRControllerOptions), (
                        WindowLength,
                        TargetBufferRatio,
                        BufferCostWeight,
                        AllowUpgrades
                    ))
}

/// A model predictive controller decides control actions by optimizing an objective function over multiple segment groups.
export class ModelPredictiveController : public BaseMultiviewABRController {
    int _windowLength;
    double _targetBufferSeconds;
    double _bufferCostWeight;
    bool _allowUpgrades;

public:
    /// Creates a model predictive controller with the specified configuration and options.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param options The options for the model predictive controller.
    explicit ModelPredictiveController(const StreamingConfig &streamingConfig,
                                       const ModelPredictiveControllerOptions &options = {}) :
        BaseMultiviewABRController(streamingConfig, options), _windowLength(options.WindowLength),
        _targetBufferSeconds(options.TargetBufferRatio * _maxBufferSeconds),
        _bufferCostWeight(options.BufferCostWeight), _allowUpgrades(options.AllowUpgrades) {
    }

    [[nodiscard]] ControlAction GetControlAction(const MultiviewABRControllerContext &context) const override {
        if (_allowUpgrades) return GetControlActionWithUpgrades(context);
        return {static_cast<int>(context.BufferedBitrateIDs.extent(0)), GetBitrateIDsWithoutUpgrades(context)};
    }

private:
    [[nodiscard]] ControlAction GetControlActionWithUpgrades(const MultiviewABRControllerContext &) const {
        // TODO: Not implemented.
        return {};
    }

    [[nodiscard]] vector<int> GetBitrateIDsWithoutUpgrades(const MultiviewABRControllerContext &context) const {
        const auto throughputMbps = context.ThroughputMbps * _throughputDiscount;
        const auto distributions = context.ViewPredictor.PredictPrimaryStreamDistributions(
            context.BufferSeconds, _windowLength, _segmentSeconds);

        // The (i, j)-th row represents the bitrate set corresponding to the j-th primary bitrate for the i-th segment group.
        mdarray<int, dims<3>> bitrateIDSets(_windowLength, _bitratesMbps.size(), _streamCount);
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            const auto primaryStreamID = static_cast<int>(ranges::max_element(distribution) - distribution.cbegin());
            const auto primaryWeight = distribution[primaryStreamID] * _primaryViewSize +
                (1 - distribution[primaryStreamID]) * _secondaryViewSize;
            for (auto primaryBitrateID = 0; primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                const span bitrateIDSet(&bitrateIDSets[groupID, primaryBitrateID, 0], _streamCount);
                for (auto streamID = 0; streamID < _streamCount; ++streamID) {
                    const auto weight = distribution[streamID] * _primaryViewSize +
                        (1 - distribution[streamID]) * _secondaryViewSize;
                    bitrateIDSet[streamID] = GetBitrateIDAbove(
                        _bitratesMbps[primaryBitrateID] * weight / primaryWeight);
                }
            }
        }

        // The (i, j)-th element represents the multiview utility of the (i, j)-th bitrate set.
        mdarray<double, dims<2>> multiviewUtilities(_windowLength, _bitratesMbps.size());
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            for (auto primaryBitrateID = 0; primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                const span bitrateIDSet(&bitrateIDSets[groupID, primaryBitrateID, 0], _streamCount);
                multiviewUtilities[groupID, primaryBitrateID] = *ranges::fold_left_first(
                    views::iota(0, _streamCount) | views::transform([&](int streamID) {
                        const auto bitrateID = bitrateIDSet[streamID];
                        return distribution[streamID] * _weightedPrimaryUtilities[bitrateID]
                            + (1 - distribution[streamID]) * _weightedSecondaryUtilities[bitrateID];
                    }), plus());
            }
        }

        // The (i, j)-th element represents the total download time of the (i, j)-th bitrate set.
        mdarray<double, dims<2>> downloadSeconds(_windowLength, _bitratesMbps.size());
        for (auto groupID = 0; groupID < _windowLength; ++groupID)
            for (auto primaryBitrateID = 0; primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                const span bitrateIDSet(&bitrateIDSets[groupID, primaryBitrateID, 0], _streamCount);
                downloadSeconds[groupID, primaryBitrateID] = *ranges::fold_left_first(
                    views::iota(0, _streamCount) | views::transform([&](int streamID) {
                        return _bitratesMbps[bitrateIDSet[streamID]];
                    }), plus()) * _segmentSeconds / throughputMbps;
            }

        const auto Objective = [&](int groupID, int primaryBitrateID, double bufferSeconds) {
            return multiviewUtilities[groupID, primaryBitrateID] - _bufferCostWeight * BufferCost(bufferSeconds);
        };

        const function<pair<optional<int>, double>(int, double, int)> GetOptPrimaryBitrateIDAndObjective =
            [&](int groupID, double bufferSeconds, int prevPrimaryBitrateID) {
            optional<int> optPrimaryBitrateID;
            auto optObjective = -numeric_limits<double>::infinity();
            for (auto primaryBitrateID = prevPrimaryBitrateID;
                 primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                bufferSeconds -= downloadSeconds[groupID, primaryBitrateID];
                if (bufferSeconds < 0) continue;

                auto objective = Objective(groupID, primaryBitrateID, bufferSeconds);
                bufferSeconds = min(bufferSeconds + _segmentSeconds, _maxBufferSeconds);
                if (groupID < _windowLength - 1) {
                    const auto [optNextPrimaryBitrateID, optFutureObjective]
                        = GetOptPrimaryBitrateIDAndObjective(groupID + 1, bufferSeconds, primaryBitrateID);
                    if (!optNextPrimaryBitrateID) continue;
                    objective += optFutureObjective;
                }
                if (objective > optObjective) optPrimaryBitrateID = primaryBitrateID, optObjective = objective;
            }
            return pair(optPrimaryBitrateID, optObjective);
        };

        const auto optPrimaryBitrateID = GetOptPrimaryBitrateIDAndObjective(0, context.BufferSeconds, 0).first;
        if (!optPrimaryBitrateID) return vector(_streamCount, 0);
        return {from_range, span(&bitrateIDSets[0, *optPrimaryBitrateID, 0], _streamCount)};
    }

    [[nodiscard]] double BufferCost(double bufferSeconds) const {
        const auto deviation = max(1 - bufferSeconds / _targetBufferSeconds, 0.);
        return deviation * deviation;
    }
};
