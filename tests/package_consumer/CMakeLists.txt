cmake_minimum_required(VERSION 3.10)
project(MCUMallocTrackerConsumer C)

set(MCUMallocTracker_PACKAGE_DIR "" CACHE PATH "Path to the installed MCUMallocTrackerConfig.cmake directory")

find_package(MCUMallocTracker CONFIG REQUIRED
    CONFIGS MCUMallocTrackerConfig.cmake
    PATHS "${MCUMallocTracker_PACKAGE_DIR}"
    NO_DEFAULT_PATH)

add_executable(package_consumer main.c)
target_link_libraries(package_consumer PRIVATE MCUMallocTracker::tracker)
