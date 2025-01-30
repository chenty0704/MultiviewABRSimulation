#include <gtest/gtest.h>

import System.Base;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;
import MultiviewABRSimulation.MultiviewABRSimulator;
import MultiviewABRSimulation.ThroughputPredictors.MovingAveragePredictor;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;
using namespace experimental;

TEST(MultiviewABRSimulatorTest, BasicSimulation) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const vector throughputsMbps = {8., 32., 24., 16.};
    const NetworkSeriesView networkSeries = {1., throughputsMbps};
    const vector<int64_t> primaryStreamIDs(40);
    const PrimaryStreamSeriesView primaryStreamSeries = {0.1, primaryStreamIDs};

    double rebufferingSeconds;
    mdarray<double, dims<2>> bitratesMbps(4, 4);
    const SimulationSeriesRef simulationSeries = {rebufferingSeconds, bitratesMbps.to_mdspan()};
    MultiviewABRSimulator::Simulate(streamingConfig, ThroughputBasedControllerOptions(),
                                    networkSeries, primaryStreamSeries, simulationSeries);
    EXPECT_DOUBLE_EQ(rebufferingSeconds, 0.);
    EXPECT_EQ(bitratesMbps.container(), vector({
                  1., 1., 1., 1.,
                  4., 1., 1., 1.,
                  4., 1., 1., 1.,
                  8., 1., 1., 1.}));
}
