cmake_minimum_required(VERSION 3.14)
project(mtv1_viewer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Platform detection ----
if(WIN32)
    set(TRANSPORT_SRC src/mtv1_transport_serial_win.c)
    set(MTV1_PLATFORM_WIN32 1)
    add_compile_definitions(MTV1_PLATFORM_WIN32=1)
else()
    set(TRANSPORT_SRC src/mtv1_transport_serial_posix.c)
    set(MTV1_PLATFORM_WIN32 0)
    add_compile_definitions(MTV1_PLATFORM_WIN32=0)
endif()

# ---- Core library (shared by viewer + tests) ----
add_library(mtv1_core STATIC
    src/mtv1_crc32.c
    src/mtv1_util.c
    src/mtv1_protocol.c
    src/mtv1_decode_mts1.c
    src/mtv1_model.c
    src/mtv1_filemap.c
    src/mtv1_render_tui.c
    src/mtv1_diff.c
    src/mtv1_transport.c
    ${TRANSPORT_SRC}
)

target_include_directories(mtv1_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(mtv1_core PUBLIC kernel32)
else()
    target_link_libraries(mtv1_core PUBLIC m)  # math library for POSIX
endif()

# ---- Viewer executable ----
add_executable(mt_viewer
    src/main.c
    src/mtv1_capture.c
    src/mtv1_replay.c
    src/mtv1_export.c
)

target_link_libraries(mt_viewer PRIVATE mtv1_core)

# ---- Test executables ----
enable_testing()

foreach(TEST_NAME test_protocol test_decode_mts1 test_diff)
    add_executable(${TEST_NAME} tests/${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} PRIVATE mtv1_core)
    target_compile_definitions(${TEST_NAME} PRIVATE MTV1_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vectors/")
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# ---- Generate test vectors (Python) ----
find_package(Python3 QUIET)
if(Python3_FOUND)
    add_custom_target(gen_vectors
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_vectors.py
                ${CMAKE_CURRENT_SOURCE_DIR}/vectors/
        COMMENT "Generating MTV1 test vectors"
    )
    # Make tests depend on vector generation
    add_dependencies(test_protocol gen_vectors)
    add_dependencies(test_decode_mts1 gen_vectors)
    add_dependencies(test_diff gen_vectors)
else()
    message(WARNING "Python3 not found - test vectors must be generated manually")
    message(STATUS "Run: python3 tools/gen_vectors.py vectors/")
endif()

# ---- Message summary ----
message(STATUS "")
message(STATUS "MTV1 Viewer Build Summary:")
message(STATUS "  Executable: mt_viewer")
message(STATUS "  Tests: test_protocol, test_decode_mts1, test_diff")
if(Python3_FOUND)
    message(STATUS "  Vector generation: Enabled (gen_vectors target)")
else()
    message(STATUS "  Vector generation: Manual (gen_vectors.py)")
endif()
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
