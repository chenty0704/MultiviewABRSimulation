export module MultiviewABRSimulation.NetworkSimulator;

import System.Base;
import System.Math;

import MultiviewABRSimulation.Base;

using namespace std;

/// Represents the options for a network simulator.
export struct NetworkSimulatorOptions {
    double UnitSeconds = 1 / 30.; /// The unit time in seconds.
};

/// Simulates download actions under a network series.
export class NetworkSimulator {
    NetworkSeriesView _networkSeries;
    double _unitSeconds;

    int _intervalID = 0;
    double _secondsInInterval = 0.;

public:
    /// Creates a network simulator from a network series with the specified options.
    /// @param networkSeries A network series.
    /// @param options The options for the network simulator.
    explicit NetworkSimulator(NetworkSeriesView networkSeries, const NetworkSimulatorOptions &options = {}):
        _networkSeries(networkSeries), _unitSeconds(options.UnitSeconds) {
    }

    /// Downloads content with the specified size.
    /// @param sizeMB The content size in megabytes.
    /// @returns The download time in seconds.
    double Download(double sizeMB) {
        return Download(sizeMB, numeric_limits<double>::infinity()).Seconds;
    }

    /// Downloads content with the specified size until a timeout.
    /// @param sizeMB The content size in megabytes.
    /// @param timeoutSeconds The timeout in seconds.
    /// @returns The downloaded size in megabytes and the download time in seconds.
    TimedValue<double> Download(double sizeMB, double timeoutSeconds) {
        const auto [intervalSeconds, throughputsMbps] = _networkSeries;
        const auto intervalCount = static_cast<int>(throughputsMbps.size());

        auto downloadSeconds = 0., downloadedMB = 0.;
        while (downloadSeconds < timeoutSeconds && downloadedMB < sizeMB) {
            const auto remSizeMB = sizeMB - downloadedMB;
            const auto remSecondsInInterval =
                min(intervalSeconds - _secondsInInterval, timeoutSeconds - downloadSeconds);
            const auto remMBInInterval = throughputsMbps[_intervalID] * remSecondsInInterval / 8;
            if (remSizeMB < remMBInInterval) {
                const auto elapsedSeconds = remSizeMB * 8 / throughputsMbps[_intervalID];
                downloadSeconds += elapsedSeconds, downloadedMB = sizeMB;
                _secondsInInterval += elapsedSeconds;
            } else {
                downloadSeconds += remSecondsInInterval, downloadedMB += remMBInInterval;
                if (_secondsInInterval + remSecondsInInterval == intervalSeconds)
                    _intervalID = (_intervalID + 1) % intervalCount, _secondsInInterval = 0.;
                else _secondsInInterval += remSecondsInInterval;
            }
        }

        downloadSeconds = Math::Ceiling(downloadSeconds, _unitSeconds);
        _secondsInInterval = Math::Ceiling(_secondsInInterval, _unitSeconds);
        return {downloadedMB, downloadSeconds};
    }

    /// Waits for a specified amount of time.
    /// @param seconds The wait time in seconds.
    void WaitFor(double seconds) {
        const auto [intervalSeconds, throughputsMbps] = _networkSeries;
        const auto intervalCount = static_cast<int>(throughputsMbps.size());

        _intervalID += Math::Floor(seconds / intervalSeconds);
        _secondsInInterval += fmod(seconds, intervalSeconds);
        if (_secondsInInterval >= intervalSeconds) ++_intervalID, _secondsInInterval -= intervalSeconds;
        _intervalID %= intervalCount;
    }
};
