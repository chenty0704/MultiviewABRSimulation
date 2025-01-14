module;

#include <System/Macros.h>

export module MultiviewABRSimulation.ThroughputPredictors.IThroughputPredictor;

/// Represents the base options for a throughput predictor.
export struct BaseThroughputPredictorOptions {
    virtual ~BaseThroughputPredictorOptions() = default;
};

export {
    DESCRIBE_STRUCT(BaseThroughputPredictorOptions, (), ())
}

/// Defines the interface of a throughput predictor.
export class IThroughputPredictor {
public:
    virtual ~IThroughputPredictor() = default;

    /// Updates the throughput predictor with the latest download information.
    /// @param downloadedMB The downloaded size in megabytes.
    /// @param downloadSeconds The download time in seconds.
    virtual void Update(double downloadedMB, double downloadSeconds) = 0;

    /// Predicts throughput in megabits per second.
    /// @returns The predicted throughput in megabits per second.
    [[nodiscard]] virtual double PredictThroughputMbps() const = 0;
};
