module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ViewPredictors.IViewPredictor;

import System.Base;
import System.MDArray;

using namespace std;
using namespace experimental;

/// Represents the configuration for a view predictor.
export struct ViewPredictorConfig {
    int StreamCount; ///< The total number of streams.
    double IntervalSeconds; ///< The interval between two values in seconds.
    double SegmentSeconds; ///< The segment duration in seconds.
};

/// Represents the base options for a view predictor.
export struct BaseViewPredictorOptions {
    virtual ~BaseViewPredictorOptions() = default;
};

export {
    DESCRIBE_STRUCT(BaseViewPredictorOptions, (), ())
}

/// Defines the interface of a view predictor.
export class IViewPredictor {
public:
    virtual ~IViewPredictor() = default;

    /// Updates the view predictor with the latest view information.
    /// @param primaryStreamIDs A list of primary stream IDs.
    virtual void Update(span<const int64_t> primaryStreamIDs) = 0;

    /// Predicts primary stream distribution.
    /// @param offsetSeconds The offset of the prediction in seconds.
    /// @returns The predicted primary stream distribution.
    [[nodiscard]] vector<double> PredictPrimaryStreamDistribution(double offsetSeconds) const {
        return PredictPrimaryStreamDistributions(offsetSeconds, 1).container();
    }

    /// Predicts a list of primary stream distributions within the specified prediction window.
    /// @param offsetSeconds The offset of the prediction window in seconds.
    /// @param groupCount The number of segment groups in the prediction window.
    /// @returns A list of primary stream distributions within the specified prediction window.
    [[nodiscard]] virtual mdarray<double, dims<2>>
    PredictPrimaryStreamDistributions(double offsetSeconds, int groupCount) const = 0;
};

/// Provides a skeletal implementation of a view predictor.
export class BaseViewPredictor : public IViewPredictor {
protected:
    int _streamCount;
    double _intervalSeconds;
    double _segmentSeconds;

    explicit BaseViewPredictor(const ViewPredictorConfig &config, const BaseViewPredictorOptions & = {}) :
        _streamCount(config.StreamCount), _intervalSeconds(config.IntervalSeconds),
        _segmentSeconds(config.SegmentSeconds) {
    }

    [[nodiscard]] mdarray<double, dims<2>> PredictStaticDistributions(int groupCount, int64_t primaryStreamID) const {
        mdarray<double, dims<2>> distributions(groupCount, _streamCount);
        for (auto groupID = 0; groupID < groupCount; ++groupID)
            distributions[groupID, primaryStreamID] = 1.;
        return distributions;
    }
};
