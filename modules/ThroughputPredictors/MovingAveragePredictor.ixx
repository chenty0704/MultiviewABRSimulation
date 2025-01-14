module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ThroughputPredictors.MovingAveragePredictor;

import System.Base;
import System.Math;

import MultiviewABRSimulation.ThroughputPredictors.IThroughputPredictor;

using namespace std;

/// Represents the options for a moving average predictor.
export struct MovingAveragePredictorOptions : BaseThroughputPredictorOptions {
    double WindowSeconds = 4.; ///< The length of the moving window in seconds.
};

export {
    DESCRIBE_STRUCT(MovingAveragePredictorOptions, (BaseThroughputPredictorOptions), (
                        WindowSeconds
                    ))
}

/// A moving average predictor predicts throughputs from the mean value and mean deviation within a moving window.
export class MovingAveragePredictor : public IThroughputPredictor {
    double _windowSeconds;

    deque<double> _intervalsSeconds, _downloadedMb;
    double _totalSeconds = 0., _totalMb = 0.;

public:
    /// Creates a moving average predictor with the specified options.
    /// @param options The options for the moving average predictor.
    explicit MovingAveragePredictor(const MovingAveragePredictorOptions &options = {}) :
        _windowSeconds(options.WindowSeconds) {
    }

    void Update(double downloadedMB, double downloadSeconds) override {
        const auto downloadedMb = downloadedMB * 8;
        _intervalsSeconds.push_back(downloadSeconds), _downloadedMb.push_back(downloadedMb);
        _totalSeconds += downloadSeconds, _totalMb += downloadedMb;

        while (_totalSeconds - _intervalsSeconds.front() > _windowSeconds) {
            _totalSeconds -= _intervalsSeconds.front(), _totalMb -= _downloadedMb.front();
            _intervalsSeconds.pop_front(), _downloadedMb.pop_front();
        }
    }

    [[nodiscard]] double PredictThroughputMbps() const override {
        if (_totalSeconds <= _windowSeconds) return _totalMb / _totalSeconds;

        const auto excessSeconds = _totalSeconds - _windowSeconds;
        const auto excessMb = _downloadedMb.front() * excessSeconds / _intervalsSeconds.front();
        return (_totalMb - excessMb) / _windowSeconds;
    }
};
