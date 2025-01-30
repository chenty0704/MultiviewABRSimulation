module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ViewPredictors.IViewPredictor;

import System.Base;
import System.MDArray;

using namespace std;
using namespace experimental;

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

    /// Predicts primary stream distribution for a segment group.
    /// @param offsetSeconds The offset of the segment group in seconds.
    /// @param segmentSeconds The segment duration in seconds.
    /// @returns The predicted primary stream distribution for the segment group.
    [[nodiscard]] vector<double> PredictPrimaryStreamDistribution(double offsetSeconds, double segmentSeconds) const {
        return PredictPrimaryStreamDistributions(offsetSeconds, 1, segmentSeconds).container();
    }

    /// Predicts a list of primary stream distributions within the specified prediction window.
    /// @param offsetSeconds The offset of the prediction window in seconds.
    /// @param groupCount The number of segment groups in the prediction window.
    /// @param segmentSeconds The segment duration in seconds.
    /// @returns A list of predicted primary stream distributions within the specified prediction window.
    [[nodiscard]] virtual mdarray<double, dims<2>>
    PredictPrimaryStreamDistributions(double offsetSeconds, int groupCount, double segmentSeconds) const = 0;
};

/// Provides a skeletal implementation of a view predictor.
export class BaseViewPredictor : public IViewPredictor {
protected:
    int _streamCount;
    double _intervalSeconds;

    BaseViewPredictor(int streamCount, double intervalSeconds, const BaseViewPredictorOptions & = {}) :
        _streamCount(streamCount), _intervalSeconds(intervalSeconds) {
    }

    [[nodiscard]] mdarray<double, dims<2>> PredictStaticDistributions(int groupCount, int64_t primaryStreamID) const {
        mdarray<double, dims<2>> distributions(groupCount, _streamCount);
        for (auto groupID = 0; groupID < groupCount; ++groupID)
            distributions[groupID, primaryStreamID] = 1.;
        return distributions;
    }
};
