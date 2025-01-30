#include <gtest/gtest.h>

import System.Base;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.NetworkSimulator;

using namespace std;

TEST(NetworkSimulatorTest, BasicSimulation) {
    const vector throughputsMbps = {8., 32., 24., 16.};
    const NetworkSeriesView networkSeries = {1., throughputsMbps};
    NetworkSimulator simulator(networkSeries);

    EXPECT_DOUBLE_EQ(simulator.Download(0.5).Seconds, 0.5);
    EXPECT_DOUBLE_EQ(simulator.Download(2.5).Seconds, 1.);
    simulator.WaitFor(1.);
    EXPECT_EQ(simulator.Download(4., 1.), (TimedValue{2.5, 1.}));
    EXPECT_EQ(simulator.Download(1.5, 2.), (TimedValue{1.5, 1.}));
}
