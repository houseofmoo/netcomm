cmake_minimum_required(VERSION 3.15)
project(eROIL_Tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Executable target
add_executable(eROIL_Tests)

# Project sources
target_sources(eROIL_Tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

# Local headers
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# libraries
target_link_libraries(eROIL_Tests PRIVATE eROIL)

# Strict warnings only for non-system code (code in src/)
target_compile_options(eROIL PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wunused-parameter -Wunused-variable>
)