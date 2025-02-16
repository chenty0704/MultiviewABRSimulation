#include <gtest/gtest.h>

import System.Base;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ThroughputBasedController;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;

TEST(ThroughputBasedControllerTest, BasicControl) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const StaticPredictor predictor(4, 1.);
    const ThroughputBasedController controller(streamingConfig);

    MultiviewABRControllerContext context = {.ViewPredictor = predictor};
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({1, 0, 0, 0}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({2, 0, 0, 0}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({3, 0, 0, 0}));
}
