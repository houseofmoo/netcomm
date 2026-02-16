cmake_minimum_required(VERSION 3.15)
project(eroil_network LANGUAGES C CXX)

# what compiler we using
message(STATUS "")
message(STATUS "==== Compiler Information ====")
message(STATUS "  C++ compiler      : ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler ID       : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Compiler version  : ${CMAKE_CXX_COMPILER_VERSION}")
#message(STATUS "  Compile features  : ${CMAKE_CXX_COMPILE_FEATURES}")
message(STATUS "")
message(STATUS "  C compiler        : ${CMAKE_C_COMPILER}")
message(STATUS "  C standard        : ${CMAKE_C_STANDARD}")
message(STATUS "  Compiler ID       : ${CMAKE_C_COMPILER_ID}")
message(STATUS "  Compiler version  : ${CMAKE_C_COMPILER_VERSION}")
#message(STATUS "  Compile features  : ${CMAKE_C_COMPILE_FEATURES}")
message(STATUS "==============================")
message(STATUS "")

# build as static lib
add_library(${PROJECT_NAME} STATIC)

# require language standards per-target
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# options
option(EROIL_ELOG_ENABLED "Enable eROIL event logging" OFF)

# macro defs
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        "EROIL_ELOG_ENABLED=$<IF:$<BOOL:${EROIL_ELOG_ENABLED}>,1,0>"
    PRIVATE
        $<$<PLATFORM_ID:Windows>:EROIL_WIN32>
        $<$<PLATFORM_ID:Linux>:EROIL_LINUX>
)

# source files
target_sources(${PROJECT_NAME} 
    PRIVATE
        #platform independent sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/root.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api/eroil_c.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api/eroil_cpp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/address/address.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/comm/connection_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log/evtlog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/manager/manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/route_table.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/router.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/transport_registry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/shm_recv.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/shm_send.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/shm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/time/time_store.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shm_recv_worker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/socket_recv_worker.cpp

        # windows only
        $<$<PLATFORM_ID:Windows>:
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/win/win_named_semaphore.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/win/win_semaphore.cpp

            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win/win_platform.cpp

            ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/win/win_shm.cpp

            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_map_err.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_socket_context.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_client.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_server.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_socket.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_udp_multicast.cpp
        >

        # linux only
        $<$<PLATFORM_ID:Linux>:
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/linux/linux_named_semaphore.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/linux/linux_semaphore.cpp
            
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/linux_platform.cpp
    
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/linux/linux_shm.cpp
            
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_map_err.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_socket_context.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_tcp_client.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_tcp_server.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_tcp_socket.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/linux/linux_udp_multicast.cpp
        >
)

# local headers
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# extensive but reasonable warnings
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:
    $<$<CXX_COMPILER_ID:MSVC>:
      /W4 /WX /permissive-
      /w14242 /w14254 /w14263 /w14265 /w14287
      /w14545 /w14546 /w14547 /w14549
    >
    $<$<CXX_COMPILER_ID:Clang>:
      -Wall -Wextra -Wpedantic -Werror
      -Wsign-conversion 
      -Wfloat-conversion
      -Wold-style-cast
      -Wdouble-promotion
      -Wshadow
      -Wformat=2
      -Wundef
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wnull-dereference
      -Wimplicit-fallthrough
      -Wsign-conversion
      -Wcast-align
      #-Wconversion # this one breaks any and all narrowing which 
                    # is used alot cause legacy code loved odd shaped ints
    >
    $<$<CXX_COMPILER_ID:GNU>:
      -Wall -Wextra -Wpedantic -Werror
      -Wsign-conversion 
      -Wfloat-conversion
      -Wold-style-cast
      -Wdouble-promotion
      -Wshadow
      -Wformat=2
      -Wundef
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wnull-dereference
      -Wimplicit-fallthrough
      -Wsign-conversion
      -Wcast-align
      -Wduplicated-cond
      -Wduplicated-branches
      -Wlogical-op
      -Wmisleading-indentation
      #-Wconversion # this one breaks any and all narrowing which 
                    # is used alot cause legacy code loved odd shaped ints
    >
  >
)

# external system libs
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Linux>:pthread>
)