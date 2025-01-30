module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ViewPredictors.StaticPredictor;

import System.Base;
import System.MDArray;

import MultiviewABRSimulation.ViewPredictors.IViewPredictor;

using namespace std;
using namespace experimental;

/// Represents the options for a static predictor.
export struct StaticPredictorOptions : BaseViewPredictorOptions {
};

export {
    DESCRIBE_STRUCT(StaticPredictorOptions, (BaseViewPredictorOptions), ())
}

/// A static predictor predicts primary stream IDs using only the previous primary stream ID.
export class StaticPredictor : public BaseViewPredictor {
    int64_t _prevPrimaryStreamID = 0;

public:
    /// Creates a static predictor with the specified configuration and options.
    /// @param streamCount The total number of streams.
    /// @param intervalSeconds The interval between two samples in seconds.
    /// @param options The options for the static predictor.
    StaticPredictor(int streamCount, double intervalSeconds, const StaticPredictorOptions &options = {}) :
        BaseViewPredictor(streamCount, intervalSeconds, options) {
    }

    void Update(span<const int64_t> primaryStreamIDs) override {
        _prevPrimaryStreamID = primaryStreamIDs.back();
    }

    [[nodiscard]] mdarray<double, dims<2>>
    PredictPrimaryStreamDistributions(double, int groupCount, double) const override {
        return PredictStaticDistributions(groupCount, _prevPrimaryStreamID);
    }
};
