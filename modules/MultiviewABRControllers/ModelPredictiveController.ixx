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
    int WindowLength = 5; ///< The number of segment groups in the optimization window.
    double TargetBufferRatio = 0.6; ///< The target buffer level (normalized by the maximum buffer level).
    double BufferCostWeight = 2.; ///< The weight of buffer cost.
    bool UpgradeAware = true; ///< Whether to consider segment upgrades.
    double UpgradeSafetyFactor = 1.5; ///< The safety factor for segment upgrades.
};

export {
    DESCRIBE_STRUCT(ModelPredictiveControllerOptions, (BaseMultiviewABRControllerOptions), (
                        WindowLength,
                        TargetBufferRatio,
                        BufferCostWeight,
                        UpgradeAware,
                        UpgradeSafetyFactor
                    ))
}

/// A model predictive controller decides control actions by optimizing an objective function over multiple segment groups.
export class ModelPredictiveController : public BaseMultiviewABRController {
    int _windowLength;
    double _targetBufferSeconds;
    double _bufferCostWeight;
    bool _upgradeAware;
    double _upgradeSafetyFactor;

public:
    /// Creates a model predictive controller with the specified configuration and options.
    /// @param streamingConfig The adaptive bitrate streaming configuration.
    /// @param options The options for the model predictive controller.
    explicit ModelPredictiveController(const StreamingConfig &streamingConfig,
                                       const ModelPredictiveControllerOptions &options = {}) :
        BaseMultiviewABRController(streamingConfig, options), _windowLength(options.WindowLength),
        _targetBufferSeconds(options.TargetBufferRatio * _maxBufferSeconds),
        _bufferCostWeight(options.BufferCostWeight), _upgradeAware(options.UpgradeAware),
        _upgradeSafetyFactor(options.UpgradeSafetyFactor) {
    }

    [[nodiscard]] ControlAction GetControlAction(const MultiviewABRControllerContext &context) override {
        if (_upgradeAware) return GetControlActionWithUpgrades(context);
        const auto bufferedGroupCount = static_cast<int>(context.BufferedBitrateIDs.extent(0));
        return {bufferedGroupCount, GetBitrateIDsWithoutUpgrades(context)};
    }

private:
    [[nodiscard]] ControlAction GetControlActionWithUpgrades(const MultiviewABRControllerContext &context) const {
        const auto throughputMbps = context.ThroughputMbps * _throughputDiscount;
        const auto bufferedBitrateIDs = context.BufferedBitrateIDs;
        const auto bufferedGroupCount = static_cast<int>(bufferedBitrateIDs.extent(0));
        const auto distributions = context.ViewPredictor.PredictPrimaryStreamDistributions(
            context.BufferSeconds - bufferedGroupCount * _segmentSeconds, _windowLength, _segmentSeconds);
        const auto bufferedPrimaryBitrateIDs = views::iota(0, bufferedGroupCount) | views::transform([&](int groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            const auto primaryStreamID = static_cast<int>(ranges::max_element(distribution) - distribution.cbegin());
            return bufferedBitrateIDs[groupID, primaryStreamID];
        }) | ranges::to<vector>();

        vector<mdarray<int, dims<2>>> bitrateIDSets(_windowLength);
        for (auto groupID = 0; groupID < bufferedGroupCount; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            const span _bufferedBitrateIDs(&bufferedBitrateIDs[groupID, 0], _streamCount);
            bitrateIDSets[groupID] = BitrateIDSets(distribution, _bufferedBitrateIDs);
        }
        for (auto groupID = bufferedGroupCount; groupID < _windowLength; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            bitrateIDSets[groupID] = BitrateIDSets(distribution);
        }

        vector<vector<double>> multiviewUtilities(_windowLength);
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const auto _bitrateIDSets = as_const(bitrateIDSets[groupID]).to_mdspan();
            const span distribution(&distributions[groupID, 0], _streamCount);
            multiviewUtilities[groupID] = MultiviewUtilities(_bitrateIDSets, distribution);
        }

        vector<vector<double>> downloadSeconds(_windowLength);
        for (auto groupID = 0; groupID < bufferedGroupCount; ++groupID) {
            const auto _bitrateIDSets = as_const(bitrateIDSets[groupID]).to_mdspan();
            const span _bufferedBitrateIDs(&bufferedBitrateIDs[groupID, 0], _streamCount);
            downloadSeconds[groupID] = DownloadSeconds(_bitrateIDSets, _bufferedBitrateIDs, throughputMbps);
        }
        for (auto groupID = bufferedGroupCount; groupID < _windowLength; ++groupID) {
            const auto _bitrateIDSets = as_const(bitrateIDSets[groupID]).to_mdspan();
            downloadSeconds[groupID] = DownloadSeconds(_bitrateIDSets, throughputMbps);
        }

        const auto Objective = [&](int groupID, int setID, double bufferSeconds) {
            return multiviewUtilities[groupID][setID] - _bufferCostWeight * BufferCost(bufferSeconds);
        };

        const function<pair<optional<int>, double>(int, double, int)> OptPrimaryBitrateIDAndObjective =
            [&](int groupID, double bufferSeconds, int minPrimaryBitrateID) {
            optional<int> optPrimaryBitrateID;
            auto optObjective = -numeric_limits<double>::infinity();
            for (auto primaryBitrateID = minPrimaryBitrateID;
                 primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                const auto _downloadSeconds = downloadSeconds[groupID][primaryBitrateID];
                if (_downloadSeconds > bufferSeconds) continue;

                auto nextBufferSeconds = bufferSeconds - _downloadSeconds;
                auto objective = Objective(groupID, primaryBitrateID, nextBufferSeconds);
                nextBufferSeconds = min(nextBufferSeconds + _segmentSeconds, _maxBufferSeconds);
                if (groupID < _windowLength - 1) {
                    const auto [optNextPrimaryBitrateID, optFutureObjective]
                        = OptPrimaryBitrateIDAndObjective(groupID + 1, nextBufferSeconds, primaryBitrateID);
                    if (!optNextPrimaryBitrateID) continue;
                    objective += optFutureObjective;
                }
                if (objective > optObjective) optPrimaryBitrateID = primaryBitrateID, optObjective = objective;
            }
            return pair(optPrimaryBitrateID, optObjective);
        };

        if (bufferedGroupCount == 0) {
            const auto optPrimaryBitrateID = OptPrimaryBitrateIDAndObjective(0, context.BufferSeconds, 0).first;
            if (!optPrimaryBitrateID) return {0, vector(_streamCount, 0)};
            return {0, {from_range, span(&bitrateIDSets[0][*optPrimaryBitrateID, 0], _streamCount)}};
        }

        const function<pair<deque<int>, double>(int, double, int)> OptSetIDsAndObjective =
            [&](int groupID, double bufferSeconds, int minPrimaryBitrateID) {
            const auto bufferedPrimaryBitrateID = bufferedPrimaryBitrateIDs[groupID];
            const auto maxDownloadSeconds =
                (bufferSeconds - (bufferedGroupCount - groupID) * _segmentSeconds) / _upgradeSafetyFactor;

            deque<int> optSetIDs;
            auto optObjective = -numeric_limits<double>::infinity();
            for (auto setID = max(minPrimaryBitrateID - bufferedPrimaryBitrateID, 0);
                 setID < bitrateIDSets[groupID].extent(0); ++setID) {
                const auto nextMinPrimaryBitrateID = setID > 0 ? setID + bufferedPrimaryBitrateID : minPrimaryBitrateID;
                const auto _downloadSeconds = downloadSeconds[groupID][setID];
                if (_downloadSeconds > maxDownloadSeconds) continue;

                const auto nextBufferSeconds = bufferSeconds - _downloadSeconds;
                auto objective = Objective(groupID, setID, nextBufferSeconds);
                if (groupID < bufferedGroupCount - 1) {
                    auto [optNextSetIDs, optFutureObjective] =
                        OptSetIDsAndObjective(groupID + 1, nextBufferSeconds, nextMinPrimaryBitrateID);
                    if (optNextSetIDs.empty()) continue;
                    if ((objective += optFutureObjective) > optObjective)
                        optSetIDs = move(optNextSetIDs), optSetIDs.push_front(setID), optObjective = objective;
                } else {
                    const auto [optNextPrimaryBitrateID, optFutureObjective]
                        = OptPrimaryBitrateIDAndObjective(groupID + 1, nextBufferSeconds, nextMinPrimaryBitrateID);
                    if (!optNextPrimaryBitrateID) continue;
                    if ((objective += optFutureObjective) > optObjective)
                        optSetIDs = {setID, *optNextPrimaryBitrateID}, optObjective = objective;
                }
            }
            return pair(optSetIDs, optObjective);
        };

        const auto optSetIDs = OptSetIDsAndObjective(0, context.BufferSeconds, 0).first;
        if (optSetIDs.empty()) return {bufferedGroupCount, vector(_streamCount, 0)};
        const auto optUpgradeSetIDs = optSetIDs | views::take(bufferedGroupCount);
        const auto it = ranges::find_if(optUpgradeSetIDs, [](int setID) { return setID > 0; });
        if (it == optUpgradeSetIDs.cend()) {
            const span optBitrateIDs(&bitrateIDSets[bufferedGroupCount][optSetIDs.back(), 0], _streamCount);
            return {bufferedGroupCount, {from_range, optBitrateIDs}};
        }
        const auto groupID = static_cast<int>(it - optUpgradeSetIDs.cbegin());
        const span optBitrateIDs(&bitrateIDSets[groupID][*it, 0], _streamCount);
        return {groupID, {from_range, optBitrateIDs}};
    }

    [[nodiscard]] vector<int> GetBitrateIDsWithoutUpgrades(const MultiviewABRControllerContext &context) const {
        const auto throughputMbps = context.ThroughputMbps * _throughputDiscount;
        const auto distributions = context.ViewPredictor.PredictPrimaryStreamDistributions(
            context.BufferSeconds, _windowLength, _segmentSeconds);

        vector<mdarray<int, dims<2>>> bitrateIDSets(_windowLength);
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            bitrateIDSets[groupID] = BitrateIDSets(distribution);
        }

        vector<vector<double>> multiviewUtilities(_windowLength);
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const auto _bitrateIDSets = as_const(bitrateIDSets[groupID]).to_mdspan();
            const span distribution(&distributions[groupID, 0], _streamCount);
            multiviewUtilities[groupID] = MultiviewUtilities(_bitrateIDSets, distribution);
        }

        vector<vector<double>> downloadSeconds(_windowLength);
        for (auto groupID = 0; groupID < _windowLength; ++groupID) {
            const auto _bitrateIDSets = as_const(bitrateIDSets[groupID]).to_mdspan();
            downloadSeconds[groupID] = DownloadSeconds(_bitrateIDSets, throughputMbps);
        }

        const auto Objective = [&](int groupID, int primaryBitrateID, double bufferSeconds) {
            return multiviewUtilities[groupID][primaryBitrateID] - _bufferCostWeight * BufferCost(bufferSeconds);
        };

        const function<pair<optional<int>, double>(int, double, int)> OptPrimaryBitrateIDAndObjective =
            [&](int groupID, double bufferSeconds, int prevPrimaryBitrateID) {
            optional<int> optPrimaryBitrateID;
            auto optObjective = -numeric_limits<double>::infinity();
            for (auto primaryBitrateID = prevPrimaryBitrateID;
                 primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
                const auto _downloadSeconds = downloadSeconds[groupID][primaryBitrateID];
                if (_downloadSeconds > bufferSeconds) continue;

                auto nextBufferSeconds = bufferSeconds - _downloadSeconds;
                auto objective = Objective(groupID, primaryBitrateID, nextBufferSeconds);
                nextBufferSeconds = min(nextBufferSeconds + _segmentSeconds, _maxBufferSeconds);
                if (groupID < _windowLength - 1) {
                    const auto [optNextPrimaryBitrateID, optFutureObjective]
                        = OptPrimaryBitrateIDAndObjective(groupID + 1, nextBufferSeconds, primaryBitrateID);
                    if (!optNextPrimaryBitrateID) continue;
                    objective += optFutureObjective;
                }
                if (objective > optObjective) optPrimaryBitrateID = primaryBitrateID, optObjective = objective;
            }
            return pair(optPrimaryBitrateID, optObjective);
        };

        const auto optPrimaryBitrateID = OptPrimaryBitrateIDAndObjective(0, context.BufferSeconds, 0).first;
        if (!optPrimaryBitrateID) return vector(_streamCount, 0);
        return {from_range, span(&bitrateIDSets[0][*optPrimaryBitrateID, 0], _streamCount)};
    }

    [[nodiscard]] mdarray<int, dims<2>> BitrateIDSets(span<const double> distribution,
                                                      span<const int> bufferedBitrateIDs) const {
        const auto primaryStreamID = static_cast<int>(ranges::max_element(distribution) - distribution.cbegin());
        const auto bufferedPrimaryBitrateID = bufferedBitrateIDs[primaryStreamID];
        const auto weights = views::iota(0, _streamCount) | views::transform([&](int streamID) {
            return distribution[streamID] * _primaryViewSize + (1 - distribution[streamID]) * _secondaryViewSize;
        }) | ranges::to<vector>();

        mdarray<int, dims<2>> bitrateIDSets(_bitratesMbps.size() - bufferedPrimaryBitrateID, _streamCount);
        ranges::copy(bufferedBitrateIDs, &bitrateIDSets[0, 0]);
        for (auto setID = 1; setID < bitrateIDSets.extent(0); ++setID) {
            const span bitrateIDSet(&bitrateIDSets[setID, 0], _streamCount);
            const auto primaryBitrateID = setID + bufferedPrimaryBitrateID;
            for (auto streamID = 0; streamID < _streamCount; ++streamID)
                bitrateIDSet[streamID] = max(
                    BitrateIDAbove(_bitratesMbps[primaryBitrateID] * weights[streamID] / weights[primaryStreamID]),
                    bufferedBitrateIDs[streamID]);
        }
        return bitrateIDSets;
    }

    [[nodiscard]] mdarray<int, dims<2>> BitrateIDSets(span<const double> distribution) const {
        const auto primaryStreamID = static_cast<int>(ranges::max_element(distribution) - distribution.cbegin());
        const auto weights = views::iota(0, _streamCount) | views::transform([&](int streamID) {
            return distribution[streamID] * _primaryViewSize + (1 - distribution[streamID]) * _secondaryViewSize;
        }) | ranges::to<vector>();

        mdarray<int, dims<2>> bitrateIDSets(_bitratesMbps.size(), _streamCount);
        for (auto primaryBitrateID = 0; primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
            const span bitrateIDSet(&bitrateIDSets[primaryBitrateID, 0], _streamCount);
            for (auto streamID = 0; streamID < _streamCount; ++streamID)
                bitrateIDSet[streamID] = BitrateIDAbove(
                    _bitratesMbps[primaryBitrateID] * weights[streamID] / weights[primaryStreamID]);
        }
        return bitrateIDSets;
    }

    [[nodiscard]] vector<double> MultiviewUtilities(mdspan<const int, dims<2>> bitrateIDSets,
                                                    span<const double> distribution) const {
        vector multiviewUtilities(bitrateIDSets.extent(0), 0.);
        for (auto setID = 0; setID < bitrateIDSets.extent(0); ++setID) {
            const span bitrateIDSet(&bitrateIDSets[setID, 0], _streamCount);
            multiviewUtilities[setID] = *ranges::fold_left_first(
                views::iota(0, _streamCount) | views::transform([&](int streamID) {
                    const auto bitrateID = bitrateIDSet[streamID];
                    return distribution[streamID] * _weightedPrimaryUtilities[bitrateID]
                        + (1 - distribution[streamID]) * _weightedSecondaryUtilities[bitrateID];
                }), plus()) * _segmentSeconds;
        }
        return multiviewUtilities;
    }

    [[nodiscard]] vector<double> DownloadSeconds(mdspan<const int, dims<2>> bitrateIDSets,
                                                 span<const int> bufferedBitrateIDs,
                                                 double throughputMbps) const {
        vector downloadSeconds(bitrateIDSets.extent(0), 0.);
        for (auto setID = 0; setID < bitrateIDSets.extent(0); ++setID) {
            const span bitrateIDSet(&bitrateIDSets[setID, 0], _streamCount);
            downloadSeconds[setID] = *ranges::fold_left_first(
                views::iota(0, _streamCount) | views::transform([&](int streamID) {
                    const auto bitrateID = bitrateIDSet[streamID];
                    return bitrateID > bufferedBitrateIDs[streamID] ? _bitratesMbps[bitrateID] : 0.;
                }), plus()) * _segmentSeconds / throughputMbps;
        }
        return downloadSeconds;
    }

    [[nodiscard]] vector<double> DownloadSeconds(mdspan<const int, dims<2>> bitrateIDSets,
                                                 double throughputMbps) const {
        vector downloadSeconds(_bitratesMbps.size(), 0.);
        for (auto primaryBitrateID = 0; primaryBitrateID < _bitratesMbps.size(); ++primaryBitrateID) {
            const span bitrateIDSet(&bitrateIDSets[primaryBitrateID, 0], _streamCount);
            downloadSeconds[primaryBitrateID] = *ranges::fold_left_first(
                views::iota(0, _streamCount) | views::transform([&](int streamID) {
                    return _bitratesMbps[bitrateIDSet[streamID]];
                }), plus()) * _segmentSeconds / throughputMbps;
        }
        return downloadSeconds;
    }

    [[nodiscard]] double BufferCost(double bufferSeconds) const {
        const auto deviation = max(1 - bufferSeconds / _targetBufferSeconds, 0.);
        return deviation * deviation;
    }
};
