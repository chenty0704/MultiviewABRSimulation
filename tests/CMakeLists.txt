add_executable(MultiviewABRSimulationTest
        "NetworkSimulatorTest.cpp")
target_link_libraries(MultiviewABRSimulationTest PRIVATE
        GTest::gtest_main
        MultiviewABRSimulation)
