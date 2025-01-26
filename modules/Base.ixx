module;

#include <System/Macros.h>

export module MultiviewABRSimulation.Base;

import LibraryLinkUtilities.TimeSeries;
import System.Base;

using namespace std;

/// Represents a value and a time duration.
/// @tparam T The type of the value.
export template<typename T>
struct TimedValue {
    T Value; ///< A value.
    double Seconds; ///< A time duration in seconds.

    bool operator==(const TimedValue &) const = default;
    bool operator!=(const TimedValue &) const = default;
};

/// Represents the configuration for a multiview adaptive bitrate streaming session.
export struct StreamingConfig {
    double SegmentSeconds; ///< The segment duration in seconds.
    vector<double> BitratesMbps; ///< A list of available bitrates in megabits per second (in ascending order).
    int StreamCount; ///< The total number of streams.
    double PrimaryViewSize; ///< The relative primary view size.
    double SecondaryViewSize; ///< The relative secondary view size.
    double MaxBufferSeconds; ///< The maximum buffer level in seconds.
};

export {
    DESCRIBE_STRUCT(StreamingConfig, (), (
                        SegmentSeconds,
                        BitratesMbps,
                        StreamCount,
                        PrimaryViewSize,
                        SecondaryViewSize,
                        MaxBufferSeconds
                    ))
}

/// Represents a constant view of a network series.
/// The values of a network series represent throughputs in megabits per second.
export using NetworkSeriesView = LLU::TimeSeriesView<double>;

/// Represents a constant view of a collection of network series.
/// The values of each network series represent throughputs in megabits per second.
export using NetworkDataView = LLU::TemporalDataView<double>;

/// Represents a constant view of a primary stream series.
/// The values of a primary stream series represent primary stream IDs.
export using PrimaryStreamSeriesView = LLU::TimeSeriesView<int64_t>;

/// Represents a constant view of a collection of primary stream series.
/// The values of each primary stream series represent primary stream IDs.
export using PrimaryStreamDataView = LLU::TemporalDataView<int64_t>;
