#include <gtest/gtest.h>

import MultiviewABRSimulation.ThroughputPredictors.EMAPredictor;

TEST(EMAPredictorTest, BasicPrediction) {
    EMAPredictor predictor;

    predictor.Update(4., 2.);
    EXPECT_DOUBLE_EQ(predictor.PredictThroughputMbps(), 16.);

    predictor.Update(16., 4);
    EXPECT_NEAR(predictor.PredictThroughputMbps(), 28., 0.5);

    predictor.Update(4., 4.);
    EXPECT_NEAR(predictor.PredictThroughputMbps(), 13., 0.5);
}
