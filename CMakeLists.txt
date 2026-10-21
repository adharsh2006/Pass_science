cmake_minimum_required(VERSION 3.16)
project(PassScienceUnifiedWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/backend/include)

add_library(workbench_core STATIC
    src/backend/core/stl_csv_parser.cpp
    src/backend/core/thermal_fea.cpp
    src/backend/core/multicore_batch.cpp
    src/backend/core/embedded_hal.cpp
    src/backend/core/backprop.cpp
)
target_link_libraries(workbench_core PUBLIC Threads::Threads)

add_executable(workbench_cli src/backend/app/workbench_cli.cpp)
target_link_libraries(workbench_cli PRIVATE workbench_core Threads::Threads)

enable_testing()
add_executable(workbench_tests tests/test_main.cpp)
target_link_libraries(workbench_tests PRIVATE workbench_core Threads::Threads)
add_test(NAME AllWorkbenchTests COMMAND workbench_tests --data-dir ${CMAKE_CURRENT_SOURCE_DIR}/data --out-dir ${CMAKE_CURRENT_SOURCE_DIR}/results)
