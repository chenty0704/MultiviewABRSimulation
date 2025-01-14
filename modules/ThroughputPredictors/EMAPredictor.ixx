module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ThroughputPredictors.EMAPredictor;

import System.Base;
import System.Math;

import MultiviewABRSimulation.ThroughputPredictors.IThroughputPredictor;

using namespace std;

/// Represents the options for an exponential moving average predictor.
export struct EMAPredictorOptions : BaseThroughputPredictorOptions {
    double SlowHalfLifeSeconds = 8.; ///< The slow half life in seconds.
    double FastHalfLifeSeconds = 2.; ///< The fast half life in seconds.
};

export {
    DESCRIBE_STRUCT(EMAPredictorOptions, (BaseThroughputPredictorOptions), (
                        SlowHalfLifeSeconds,
                        FastHalfLifeSeconds
                    ))
}

/// An exponential moving average predictor predicts throughputs from two exponential moving average estimates.
export class EMAPredictor : public IThroughputPredictor {
    double _slowHalfLifeSeconds, _fastHalfLifeSeconds;

    double _totalSeconds = 0.;
    double _slowPredictionMbps = 0., _fastPredictionMbps = 0.;

public:
    /// Creates an exponential moving average predictor with the specified options.
    /// @param options The options for the exponential moving average predictor.
    explicit EMAPredictor(const EMAPredictorOptions &options = {}) :
        _slowHalfLifeSeconds(options.SlowHalfLifeSeconds), _fastHalfLifeSeconds(options.FastHalfLifeSeconds) {
    }

    void Update(double downloadedMB, double downloadSeconds) override {
        const auto throughputMbps = downloadedMB * 8 / downloadSeconds;
        const auto prevSlowWeight = Math::Pow(0.5, downloadSeconds / _slowHalfLifeSeconds),
                   prevFastWeight = Math::Pow(0.5, downloadSeconds / _fastHalfLifeSeconds);
        _totalSeconds += downloadSeconds;
        _slowPredictionMbps = prevSlowWeight * _slowPredictionMbps + (1 - prevSlowWeight) * throughputMbps;
        _fastPredictionMbps = prevFastWeight * _fastPredictionMbps + (1 - prevFastWeight) * throughputMbps;
    }

    [[nodiscard]] double PredictThroughputMbps() const override {
        const auto slowFactor = 1 - Math::Pow(0.5, _totalSeconds / _slowHalfLifeSeconds),
                   fastFactor = 1 - Math::Pow(0.5, _totalSeconds / _fastHalfLifeSeconds);
        return min(_slowPredictionMbps / slowFactor, _fastPredictionMbps / fastFactor);
    }
};
