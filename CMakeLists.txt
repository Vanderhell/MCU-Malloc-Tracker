cmake_minimum_required(VERSION 3.10)
project(MCU_Malloc_Tracker C)

# ============================================================================
# Configuration
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(mcu_malloc_tracker
    PRIVATE
        MT_MAX_ALLOCS=${MT_MAX_ALLOCS}
        MT_MAX_HOTSPOTS=${MT_MAX_HOTSPOTS}
        MT_ENABLE_SNAPSHOT=${MT_ENABLE_SNAPSHOT}
        MT_ENABLE_HOTSPOTS=${MT_ENABLE_HOTSPOTS}
        MT_ENABLE_LEAK_DUMP=${MT_ENABLE_LEAK_DUMP}
        MT_FILE_ID_MODE=${MT_FILE_ID_MODE}
)

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
