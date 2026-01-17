cmake_minimum_required(VERSION 3.15)
project(eROIL LANGUAGES C CXX)

# build as static lib
add_library(eROIL STATIC)

# require language standards per-target
target_compile_features(eROIL PUBLIC cxx_std_17)
set_target_properties(eROIL PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_EXTENSIONS NO
)

# set_target_properties(eROIL PROPERTIES
#     C_STANDARD 11
#     C_STANDARD_REQUIRED YES
#     C_EXTENSIONS NO
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED YES
#     CXX_EXTENSIONS NO
# )

# options
option(EROIL_ELOG_ENABLED "Enable eROIL event logging" OFF)

# macro defs
target_compile_definitions(eROIL
    PRIVATE
        EROIL_ELOG_ENABLED=$<BOOL:${EROIL_ELOG_ENABLED}>
        $<$<PLATFORM_ID:Windows>:EROIL_WIN32>
        $<$<PLATFORM_ID:Linux>:EROIL_LINUX>
)

# platform independent sources
target_sources(eROIL 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/eroil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/address/address.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/conn/connection_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log/evtlog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/manager/manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/route_table.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/router.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router/transport_registry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/timer/time_log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/timer/timer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/send_worker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shm_recv_worker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/socket_recv_worker.cpp
)

# platform dependent sources
target_sources(eROIL 
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/win/win_named_event.cpp
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

        $<$<PLATFORM_ID:Linux>:
            ${CMAKE_CURRENT_SOURCE_DIR}/src/events/linux/linux_named_event.cpp
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
target_include_directories(eROIL 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/address
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/config
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/conn
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/events
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/manager
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/router
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/shm
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/socket
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/types
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/workers

    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        #${CMAKE_CURRENT_SOURCE_DIR}/include/eROIL
)

# strict warnings only for non-system code
target_compile_options(eROIL 
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wunused-parameter -Wunused-variable>
)

# external system libs
target_link_libraries(eROIL 
    PRIVATE
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Linux>:pthread>
)