#include <gtest/gtest.h>

import System.Base;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ModelPredictiveController;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;
using namespace experimental;

TEST(ModelPredictiveControllerTest, BasicControlWithoutUpgrades) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const StaticPredictor predictor(4, 1.);
    ModelPredictiveControllerOptions options;
    options.AllowUpgrades = false;
    const ModelPredictiveController controller(streamingConfig, options);

    const vector lastBitrateIDs = {2, 0, 0, 0};
    MultiviewABRControllerContext context = {.LastBitrateIDs = lastBitrateIDs, .ViewPredictor = predictor};
    context.BufferSeconds = 2.;
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({0, 0, 0, 0}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({1, 0, 0, 0}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({3, 0, 0, 0}));

    context.BufferSeconds = 4.;
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({2, 0, 0, 0}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({2, 0, 0, 0}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({3, 0, 0, 0}));
}
