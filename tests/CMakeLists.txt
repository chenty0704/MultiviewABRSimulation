add_executable(ThroughputPredictorsTest
        "ThroughputPredictors/EMAPredictorTest.cpp"
        "ThroughputPredictors/MovingAveragePredictorTest.cpp")
target_link_libraries(ThroughputPredictorsTest PRIVATE
        GTest::gtest_main
        ThroughputPredictors)

add_executable(ViewPredictorsTest
        "ViewPredictors/MarkovPredictorTest.cpp")
target_link_libraries(ViewPredictorsTest PRIVATE
        GTest::gtest_main
        ViewPredictors)

add_executable(MultiviewABRControllersTest
        "MultiviewABRControllers/ModelPredictiveControllerTest.cpp"
        "MultiviewABRControllers/ThroughputBasedControllerTest.cpp")
target_link_libraries(MultiviewABRControllersTest PRIVATE
        GTest::gtest_main
        MultiviewABRControllers)

add_executable(MultiviewABRSimulationTest
        "MultiviewABRSimulatorTest.cpp"
        "NetworkSimulatorTest.cpp")
target_link_libraries(MultiviewABRSimulationTest PRIVATE
        GTest::gtest_main
        MultiviewABRSimulation)
