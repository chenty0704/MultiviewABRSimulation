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

/// Represents a video stream.
export struct VideoModel {
    double SegmentSeconds; ///< The segment duration in seconds.
    vector<double> BitratesMbps; ///< The available bitrates in megabits per second (in ascending order).
};

export {
    DESCRIBE_STRUCT(VideoModel, (), (
                        SegmentSeconds,
                        BitratesMbps
                    ))
}

/// Represents a constant view of a regular network series.
/// The values of a network series represent throughputs in megabits per second.
export using NetworkSeriesView = LLU::TimeSeriesView<double>;

/// Represents a constant view of a collection of regular network series.
/// The values of each network series represent throughputs in megabits per second.
export using NetworkDataView = LLU::TemporalDataView<double>;
