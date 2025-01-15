add_library(Base)
target_sources(Base PUBLIC FILE_SET CXX_MODULES FILES "Base.ixx")
target_link_libraries(Base PUBLIC
        LibraryLinkUtilities::LibraryLinkUtilities
        System::System)

add_library(ThroughputPredictors)
target_sources(ThroughputPredictors PUBLIC FILE_SET CXX_MODULES FILES
        "ThroughputPredictors/EMAPredictor.ixx"
        "ThroughputPredictors/IThroughputPredictor.ixx"
        "ThroughputPredictors/MovingAveragePredictor.ixx"
        "ThroughputPredictors/ThroughputPredictorFactory.ixx")
target_link_libraries(ThroughputPredictors PUBLIC
        System::System
        Base)

add_library(ViewPredictors)
target_sources(ViewPredictors PUBLIC FILE_SET CXX_MODULES FILES
        "ViewPredictors/IViewPredictor.ixx"
        "ViewPredictors/MarkovPredictor.ixx"
        "ViewPredictors/StaticPredictor.ixx"
        "ViewPredictors/ViewPredictorFactory.ixx")
target_link_libraries(ViewPredictors PUBLIC
        System::System
        Base)

add_library(MultiviewABRSimulation)
target_sources(MultiviewABRSimulation PUBLIC FILE_SET CXX_MODULES FILES
        "NetworkSimulator.ixx")
target_link_libraries(MultiviewABRSimulation PUBLIC
        System::System
        Base)
