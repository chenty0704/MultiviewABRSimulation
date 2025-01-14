add_library(Base)
target_sources(Base PUBLIC FILE_SET CXX_MODULES FILES "Base.ixx")
target_link_libraries(Base PUBLIC
        LibraryLinkUtilities::LibraryLinkUtilities
        System::System)

add_library(MultiviewABRSimulation)
target_sources(MultiviewABRSimulation PUBLIC FILE_SET CXX_MODULES FILES
        "NetworkSimulator.ixx")
target_link_libraries(MultiviewABRSimulation PUBLIC
        System::System
        Base)
