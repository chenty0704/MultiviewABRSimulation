add_executable(ThroughputPredictorsTest
        "ThroughputPredictors/EMAPredictorTest.cpp"
        "ThroughputPredictors/MovingAveragePredictorTest.cpp")
target_link_libraries(ThroughputPredictorsTest PRIVATE
        GTest::gtest_main
        ThroughputPredictors)

add_executable(MultiviewABRSimulationTest
        "NetworkSimulatorTest.cpp")
target_link_libraries(MultiviewABRSimulationTest PRIVATE
        GTest::gtest_main
        MultiviewABRSimulation)
