module;

#include <System/Macros.h>

export module MultiviewABRSimulation.Base;

import LibraryLinkUtilities.TimeSeries;
import System.Base;

using namespace std;

/// The number of bits in a byte.
export inline constexpr int BitCountPerByte = 8;

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
