cmake_minimum_required(VERSION 3.15)
project(eROIL_Tests LANGUAGES C CXX)

# Executable target
add_executable(eROIL_Tests)

# require language standards per-target
target_compile_features(eROIL_Tests PUBLIC cxx_std_17)
set_target_properties(eROIL_Tests PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_EXTENSIONS NO
)

# linux vs windows macro defs
target_compile_definitions(eROIL_Tests
    PRIVATE
        $<$<PLATFORM_ID:Windows>:EROIL_WIN32>
        $<$<PLATFORM_ID:Linux>:EROIL_LINUX>
)

# Project sources
target_sources(eROIL_Tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

# Local headers
target_include_directories(eROIL_Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# libraries
target_link_libraries(eROIL_Tests PRIVATE eROIL)

# Strict warnings only for non-system code (code in src/)
target_compile_options(eROIL_Tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wunused-parameter -Wunused-variable>
)