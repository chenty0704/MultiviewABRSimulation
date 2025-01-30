#include <gtest/gtest.h>

import System.Base;

import MultiviewABRSimulation.ViewPredictors.MarkovPredictor;

using namespace std;

TEST(MarkovPredictorTest, BasicPrediction) {
    MarkovPredictorOptions options;
    options.WindowSeconds = 4.;
    MarkovPredictor predictor(4, 1., options);

    vector<int64_t> primaryStreamIDs = {0, 0};
    predictor.Update(primaryStreamIDs);
    auto predictedDistributions = predictor.PredictPrimaryStreamDistributions(0., 2, 1.);
    EXPECT_EQ(predictedDistributions.container(), vector({
                  1., 0., 0., 0.,
                  1., 0., 0., 0.}));

    primaryStreamIDs = {1, 2};
    predictor.Update(primaryStreamIDs);
    predictedDistributions = predictor.PredictPrimaryStreamDistributions(0., 2, 1.);
    EXPECT_NEAR((predictedDistributions[0, 1]), 0.11, 0.005);
    EXPECT_NEAR((predictedDistributions[0, 2]), 0.89, 0.005);
    EXPECT_NEAR((predictedDistributions[1, 1]), 0.26, 0.005);
    EXPECT_NEAR((predictedDistributions[1, 2]), 0.74, 0.005);

    primaryStreamIDs = {3, 3, 0, 0};
    predictor.Update(primaryStreamIDs);
    predictedDistributions = predictor.PredictPrimaryStreamDistributions(0., 2, 1.);
    EXPECT_NEAR((predictedDistributions[0, 0]), 0.89, 0.005);
    EXPECT_NEAR((predictedDistributions[0, 3]), 0.11, 0.005);
    EXPECT_NEAR((predictedDistributions[1, 0]), 0.74, 0.005);
    EXPECT_NEAR((predictedDistributions[1, 3]), 0.26, 0.005);
}
