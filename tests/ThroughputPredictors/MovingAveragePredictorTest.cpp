#include <gtest/gtest.h>

import MultiviewABRSimulation.ThroughputPredictors.MovingAveragePredictor;

TEST(MovingAveragePredictorTest, BasicPrediction) {
    MovingAveragePredictor predictor;

    predictor.Update(4., 2.);
    EXPECT_DOUBLE_EQ(predictor.PredictThroughputMbps(), 16.);

    predictor.Update(2., 2.);
    EXPECT_DOUBLE_EQ(predictor.PredictThroughputMbps(), 12.);

    predictor.Update(6., 2.);
    EXPECT_DOUBLE_EQ(predictor.PredictThroughputMbps(), 16.);

    predictor.Update(4., 4.);
    EXPECT_DOUBLE_EQ(predictor.PredictThroughputMbps(), 8.);
}
