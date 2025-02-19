#include <gtest/gtest.h>

import System.Base;
import System.MDArray;

import MultiviewABRSimulation.Base;
import MultiviewABRSimulation.MultiviewABRControllers.IMultiviewABRController;
import MultiviewABRSimulation.MultiviewABRControllers.ModelPredictiveController;
import MultiviewABRSimulation.ViewPredictors.StaticPredictor;

using namespace std;
using namespace experimental;

TEST(ModelPredictiveControllerTest, BasicControlWithUpgrades) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const StaticPredictor predictor(4, 1.);
    ModelPredictiveController controller(streamingConfig);

    MultiviewABRControllerContext context = {.ViewPredictor = predictor};
    context.BufferSeconds = 2.;
    vector bufferedBitrateIDs = {2, 0, 0, 0};
    context.BufferedBitrateIDs = mdspan<const int, dims<2>>(bufferedBitrateIDs.data(), 1, 4);
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {0, 0, 0, 0}}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {1, 0, 0, 0}}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {2, 0, 0, 0}}));

    context.BufferSeconds = 4.;
    bufferedBitrateIDs = {
        2, 0, 0, 0,
        2, 0, 0, 0,
        2, 0, 0, 0
    };
    context.BufferedBitrateIDs = mdspan<const int, dims<2>>(bufferedBitrateIDs.data(), 3, 4);

    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{3, {2, 0, 0, 0}}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{3, {3, 0, 0, 0}}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {3, 0, 0, 0}}));

    context.BufferSeconds = 2.;
    bufferedBitrateIDs = {0, 2, 0, 0};
    context.BufferedBitrateIDs = mdspan<const int, dims<2>>(bufferedBitrateIDs.data(), 1, 4);
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {0, 0, 0, 0}}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {1, 0, 0, 0}}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{1, {2, 0, 0, 0}}));

    context.BufferSeconds = 4.;
    bufferedBitrateIDs = {
        0, 2, 0, 0,
        0, 2, 0, 0,
        0, 2, 0, 0
    };
    context.BufferedBitrateIDs = mdspan<const int, dims<2>>(bufferedBitrateIDs.data(), 3, 4);

    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{3, {2, 0, 0, 0}}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{0, {2, 2, 0, 0}}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context), (ControlAction{0, {2, 2, 0, 0}}));
}

TEST(ModelPredictiveControllerTest, BasicControlWithoutUpgrades) {
    const StreamingConfig streamingConfig = {1., {1., 2., 4., 8.}, 4, 0.75, 5.};
    const StaticPredictor predictor(4, 1.);
    ModelPredictiveControllerOptions options;
    options.BufferCostWeight = 1.;
    options.UpgradeAware = false;
    ModelPredictiveController controller(streamingConfig, options);

    MultiviewABRControllerContext context = {.ViewPredictor = predictor};
    context.BufferSeconds = 2.;
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({0, 0, 0, 0}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({1, 0, 0, 0}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({2, 0, 0, 0}));

    context.BufferSeconds = 4.;
    context.ThroughputMbps = 5.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({1, 0, 0, 0}));

    context.ThroughputMbps = 10.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({3, 0, 0, 0}));

    context.ThroughputMbps = 15.;
    EXPECT_EQ(controller.GetControlAction(context).BitrateIDs, vector({3, 0, 0, 0}));
}
