module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ViewPredictors.MarkovPredictor;

import System.Base;
import System.Math;
import System.MDArray;

import MultiviewABRSimulation.ViewPredictors.IViewPredictor;

using namespace std;
using namespace experimental;

/// Represents the options for a Markov predictor.
export struct MarkovPredictorOptions : BaseViewPredictorOptions {
    double WindowSeconds = 30.; ///< The length of the moving window in seconds.
};

export {
    DESCRIBE_STRUCT(MarkovPredictorOptions, (BaseViewPredictorOptions), (
                        WindowSeconds
                    ))
}

/// A Markov predictor predicts primary stream distributions from a continuous-time Markov process.
export class MarkovPredictor : public BaseViewPredictor {
    double _windowSeconds;

    deque<int64_t> _primaryStreamIDs = {0};
    vector<int> _transitionCounts;

public:
    /// Creates a Markov predictor with the specified configuration and options.
    /// @param streamCount The total number of streams.
    /// @param intervalSeconds The interval between two samples in seconds.
    /// @param options The options for the Markov predictor.
    MarkovPredictor(int streamCount, double intervalSeconds, const MarkovPredictorOptions &options = {}) :
        BaseViewPredictor(streamCount, intervalSeconds, options),
        _windowSeconds(options.WindowSeconds), _transitionCounts(_streamCount) {
    }

    void Update(span<const int64_t> primaryStreamIDs) override {
        if (primaryStreamIDs.front() != _primaryStreamIDs.back()) ++_transitionCounts[primaryStreamIDs.front()];
        for (auto i = 1; i < primaryStreamIDs.size(); ++i)
            if (primaryStreamIDs[i] != primaryStreamIDs[i - 1]) ++_transitionCounts[primaryStreamIDs[i]];
        _primaryStreamIDs.append_range(primaryStreamIDs);

        const auto windowLength = Math::Round(_windowSeconds / _intervalSeconds);
        if (_primaryStreamIDs.size() > windowLength + 1) {
            const auto excessCount = static_cast<int>(_primaryStreamIDs.size()) - windowLength - 1;
            for (auto i = 1; i <= excessCount; ++i)
                if (_primaryStreamIDs[i] != _primaryStreamIDs[i - 1]) --_transitionCounts[_primaryStreamIDs[i]];
            _primaryStreamIDs.erase(_primaryStreamIDs.cbegin(), _primaryStreamIDs.cbegin() + excessCount);
        }
    }

    [[nodiscard]] mdarray<double, dims<2>>
    PredictPrimaryStreamDistributions(double offsetSeconds, int groupCount, double segmentSeconds) const override {
        const auto prevPrimaryStreamID = _primaryStreamIDs.back();
        const auto rates = _transitionCounts |
            views::transform([&](int count) { return count / _windowSeconds; }) | ranges::to<vector>();
        const auto totalRate = Math::Total(rates);
        if (totalRate == 0.) return PredictStaticDistributions(groupCount, prevPrimaryStreamID);

        mdarray<double, dims<2>> distributions(groupCount, _streamCount);
        const auto relRates = rates / totalRate;
        const auto expValues = views::iota(0, groupCount + 1) | views::transform([&](int groupID) {
            return Math::Exp(-totalRate * (offsetSeconds + groupID * segmentSeconds));
        }) | ranges::to<vector>();
        for (auto groupID = 0; groupID < groupCount; ++groupID) {
            const span distribution(&distributions[groupID, 0], _streamCount);
            const auto diffValue = (expValues[groupID] - expValues[groupID + 1]) / (totalRate * segmentSeconds);
            for (auto streamID = 0; streamID < _streamCount; ++streamID)
                distribution[streamID] = streamID != prevPrimaryStreamID
                                             ? relRates[streamID] - relRates[streamID] * diffValue
                                             : relRates[streamID] + (1 - relRates[streamID]) * diffValue;
        }
        return distributions;
    }
};
