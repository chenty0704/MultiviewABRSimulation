add_library(MultiviewABRSimulationLink SHARED "MultiviewABRSimulationLink.cpp")
target_link_libraries(MultiviewABRSimulationLink PRIVATE
        LibraryLinkUtilities::LibraryLinkUtilities
        System::System
        MultiviewABRSimulation)
