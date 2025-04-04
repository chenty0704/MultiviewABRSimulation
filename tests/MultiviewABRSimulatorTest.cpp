#include <gtest/gtest.h>

import System.Base;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;
import MultiviewABRSimulation.MultiviewABRSimulator;

using namespace std;
using namespace experimental;

TEST(MultiviewABRSimulatorTest, BasicSimulation) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const vector throughputsMbps = {8., 32., 24., 16.};
    const NetworkSeriesView networkSeries = {1., throughputsMbps};
    const vector<int64_t> primaryStreamIDs(20);
    const PrimaryStreamSeriesView primaryStreamSeries = {0.2, primaryStreamIDs};

    double rebufferingSeconds;
    mdarray<double, dims<2>> bufferedBitratesMbps(4, 4);
    mdarray<double, dims<2>> distributions(4, 4);
    double downloadedMB, rawWastedMB;
    const SimulationSeriesRef simulationSeries =
        {rebufferingSeconds, bufferedBitratesMbps.to_mdspan(), distributions.to_mdspan(), downloadedMB, rawWastedMB};
    MultiviewABRSimulator::Simulate(streamingConfig, ThroughputBasedControllerOptions(),
                                    networkSeries, primaryStreamSeries, simulationSeries);
    EXPECT_DOUBLE_EQ(rebufferingSeconds, 0.);
    EXPECT_EQ(bufferedBitratesMbps.container(), vector({
                  1., 1., 1., 1.,
                  4., 1., 1., 1.,
                  4., 1., 1., 1.,
                  8., 1., 1., 1.}));
    EXPECT_EQ(distributions.container(), vector({
                  1., 0., 0., 0.,
                  1., 0., 0., 0.,
                  1., 0., 0., 0.,
                  1., 0., 0., 0.}));
    EXPECT_DOUBLE_EQ(downloadedMB, 3.625);
    EXPECT_DOUBLE_EQ(rawWastedMB, 0.);
}
