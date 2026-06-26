cmake_minimum_required(VERSION 3.10)
project(MCU_Malloc_Tracker VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

# ============================================================================
# Configuration
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Default values for config (can be overridden from command line)
if(NOT MT_MAX_ALLOCS)
    set(MT_MAX_ALLOCS 512)
endif()

if(NOT MT_MAX_HOTSPOTS)
    set(MT_MAX_HOTSPOTS 64)
endif()

if(NOT MT_ENABLE_SNAPSHOT)
    set(MT_ENABLE_SNAPSHOT 1)
endif()

if(NOT MT_ENABLE_HOTSPOTS)
    set(MT_ENABLE_HOTSPOTS 1)
endif()

if(NOT MT_ENABLE_LEAK_DUMP)
    set(MT_ENABLE_LEAK_DUMP 1)
endif()

if(NOT MT_FILE_ID_MODE)
    set(MT_FILE_ID_MODE 1)
endif()

# ============================================================================
# Main Library: mcu_malloc_tracker
# ============================================================================

add_library(mcu_malloc_tracker STATIC
    src/mt_core.c
    src/mt_heap_stats.c
    src/mt_fragmentation.c
    src/mt_hotspots.c
    src/mt_crc32_ieee.c
    src/mt_snapshot.c
    src/mt_dump.c
    src/mtv1_stream.c
)

target_include_directories(mcu_malloc_tracker
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(mcu_malloc_tracker
    PUBLIC
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=${MT_ENABLE_SNAPSHOT}
        MT_ENABLE_HOTSPOTS=${MT_ENABLE_HOTSPOTS}
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
)

set_target_properties(mcu_malloc_tracker PROPERTIES
    EXPORT_NAME tracker
)

add_library(MCUMallocTracker::tracker ALIAS mcu_malloc_tracker)

# ============================================================================
# Self-Test Executable (Phase 2)
# ============================================================================

add_executable(selftest_phase2
    examples/selftest/main.c
)

target_include_directories(selftest_phase2
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(selftest_phase2
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=${MT_ENABLE_SNAPSHOT}
        MT_ENABLE_HOTSPOTS=${MT_ENABLE_HOTSPOTS}
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
)

target_link_libraries(selftest_phase2
    PRIVATE
        mcu_malloc_tracker
)

# ============================================================================
# Self-Test Executable (Phase 3)
# ============================================================================

add_executable(selftest_phase3
    examples/selftest_phase3/main.c
)

target_include_directories(selftest_phase3
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(selftest_phase3
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=${MT_ENABLE_SNAPSHOT}
        MT_ENABLE_HOTSPOTS=${MT_ENABLE_HOTSPOTS}
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
        MT_PLATFORM_HEAP_WALK=0
)

target_link_libraries(selftest_phase3
    PRIVATE
        mcu_malloc_tracker
)

# ============================================================================
# Self-Test Executable (Phase 4)
# ============================================================================

add_executable(selftest_phase4
    examples/selftest_phase4/main.c
)

target_include_directories(selftest_phase4
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(selftest_phase4
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=${MT_ENABLE_SNAPSHOT}
        MT_ENABLE_HOTSPOTS=1
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
        MT_PLATFORM_HEAP_WALK=0
)

target_link_libraries(selftest_phase4
    PRIVATE
        mcu_malloc_tracker
)

# ============================================================================
# Print Configuration
# ============================================================================

message(STATUS "MCU Malloc Tracker Configuration:")
message(STATUS "  MT_MAX_ALLOCS       = ${MT_MAX_ALLOCS}")
message(STATUS "  MT_MAX_HOTSPOTS     = ${MT_MAX_HOTSPOTS}")
message(STATUS "  MT_ENABLE_SNAPSHOT  = ${MT_ENABLE_SNAPSHOT}")
message(STATUS "  MT_ENABLE_HOTSPOTS  = ${MT_ENABLE_HOTSPOTS}")
message(STATUS "  MT_ENABLE_LEAK_DUMP = ${MT_ENABLE_LEAK_DUMP}")
message(STATUS "  MT_FILE_ID_MODE     = ${MT_FILE_ID_MODE}")
message(STATUS "")
message(STATUS "Primary Target:")
message(STATUS "  - snapshot_demo (run: ./build/Debug/snapshot_demo)")
message(STATUS "")
message(STATUS "Other Targets:")
message(STATUS "  - mcu_malloc_tracker (library)")
message(STATUS "  - selftest_phase* (internal unit tests, optional)")

# ============================================================================
# Self-Test Executable (Phase 5)
# ============================================================================

add_executable(selftest_phase5
    examples/selftest_phase5/main.c
)

target_include_directories(selftest_phase5
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(selftest_phase5
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=1
        MT_ENABLE_HOTSPOTS=1
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
        MT_PLATFORM_HEAP_WALK=0
)

target_link_libraries(selftest_phase5
    PRIVATE
        mcu_malloc_tracker
)

# ============================================================================
# Snapshot Demo (Primary Workflow)
# ============================================================================

add_executable(snapshot_demo
    examples/generate_snapshot/main.c
)

target_include_directories(snapshot_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(snapshot_demo
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=1
        MT_ENABLE_HOTSPOTS=1
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
)

target_link_libraries(snapshot_demo
    PRIVATE
        mcu_malloc_tracker
)

# ============================================================================
# Package install/export
# ============================================================================

set(MCUMallocTracker_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/MCUMallocTracker")

install(TARGETS mcu_malloc_tracker
    EXPORT MCUMallocTrackerTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT MCUMallocTrackerTargets
    NAMESPACE MCUMallocTracker::
    FILE MCUMallocTrackerTargets.cmake
    DESTINATION ${MCUMallocTracker_INSTALL_CMAKEDIR}
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MCUMallocTrackerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MCUMallocTrackerConfig.cmake
    INSTALL_DESTINATION ${MCUMallocTracker_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MCUMallocTrackerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MCUMallocTrackerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MCUMallocTrackerConfigVersion.cmake
    DESTINATION ${MCUMallocTracker_INSTALL_CMAKEDIR}
)

# ============================================================================
# Focused verification tests
# ============================================================================

function(mt_expect_compile_fail test_name source_path)
    set(_obj_dir "${CMAKE_CURRENT_BINARY_DIR}/compile_fail/${test_name}")
    file(MAKE_DIRECTORY "${_obj_dir}")

    if(MSVC)
        set(_obj "${_obj_dir}/${test_name}.obj")
        execute_process(
            COMMAND "${CMAKE_C_COMPILER}" /nologo /TC /c "${source_path}" /Fo"${_obj}" /I"${CMAKE_CURRENT_SOURCE_DIR}/include"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _output
            ERROR_VARIABLE _output
        )
    else()
        set(_obj "${_obj_dir}/${test_name}.o")
        execute_process(
            COMMAND "${CMAKE_C_COMPILER}" -std=c99 -Wall -Wextra -Wpedantic -Werror -I"${CMAKE_CURRENT_SOURCE_DIR}/include" -c "${source_path}" -o "${_obj}"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _output
            ERROR_VARIABLE _output
        )
    endif()

    if(_result EQUAL 0)
        message(FATAL_ERROR "Expected compile failure for ${test_name}, but compilation succeeded")
    endif()

    message(STATUS "Compile-fail verified: ${test_name}")
endfunction()

if(BUILD_TESTING)
    mt_expect_compile_fail(
        invalid_hook_config
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail_invalid_hook_config.c"
    )

    mt_expect_compile_fail(
        invalid_wrapper_predefinition
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail_invalid_wrapper_predefinition.c"
    )

    add_executable(wrapper_opt_out tests/wrapper_opt_out.c)
    target_link_libraries(wrapper_opt_out PRIVATE mcu_malloc_tracker)
    add_test(NAME wrapper_opt_out COMMAND wrapper_opt_out)

    add_executable(wrapper_multitu
        tests/wrapper_multitu_main.c
        tests/wrapper_multitu_plain.c
        tests/wrapper_multitu_wrapped.c
    )
    target_link_libraries(wrapper_multitu PRIVATE mcu_malloc_tracker)
    add_test(NAME wrapper_multitu COMMAND wrapper_multitu)
endif()

# ============================================================================
# MTV1 Streamer Selftest (optional — for advanced MTV1 workflow)
# ============================================================================

option(MTV1_TESTS "Build MTV1 streaming tests (experimental)" OFF)

if(MTV1_TESTS)
    add_executable(selftest_mtv1_streamer
        examples/selftest_mtv1_streamer/main.c
    )

    target_include_directories(selftest_mtv1_streamer
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(selftest_mtv1_streamer
        PRIVATE
            mcu_malloc_tracker
    )
endif()

# ============================================================================
# MTV1 Stream Viewer Tool (optional)
# ============================================================================

option(MTV1_VIEWER "Build MTV1 stream viewer tool (mt_viewer_c)" OFF)

if(MTV1_VIEWER)
    add_subdirectory(tools/mt_viewer_c)
    message(STATUS "MTV1 Viewer tool: ENABLED")
else()
    message(STATUS "MTV1 Viewer tool: DISABLED (use -DMTV1_VIEWER=ON to enable)")
endif()
