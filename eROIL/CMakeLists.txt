cmake_minimum_required(VERSION 3.15)
project(eROIL LANGUAGES C CXX)

# build as static lib
add_library(eROIL STATIC)

# linux vs windows macro defs
target_compile_definitions(eROIL
    PRIVATE
        $<$<PLATFORM_ID:Windows>:EROIL_WIN32>
        $<$<PLATFORM_ID:Linux>:EROIL_LINUX>
)

# platform independent sources
target_sources(eROIL PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eroil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/address/address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/broadcast/broadcast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/comms/recvrs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/manager/manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/router/dispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/router/route_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/router/router.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/router/transport_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/send_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shm_recv_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/socket_recv_worker.cpp
)

# platform dependent sources
target_sources(eROIL PRIVATE
    $<$<PLATFORM_ID:Windows>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/events/win/win_named_event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/events/win/win_semaphore.cpp

        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/win/win_context.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/win/win_socket_tcp.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/win/win_socket_udp.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/win/win_socket_udpm.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/win/win_socket.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/win/win_shm_recv.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/win/win_shm_send.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/win/win_shm.cpp
        
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_map_err.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_socket_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_tcp_socket.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket/win/win_udp_multicast.cpp
    >
    $<$<PLATFORM_ID:Linux>:
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/named_event/linux/linux_named_event.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/linux/linux_context.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/linux/linux_socket_tcp.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/linux/linux_socket_udp.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/linux/linux_socket_udpm.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/net/linux/linux_socket.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/linux/linux_shm_recv.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/linux/linux_shm_send.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/shm/linux/linux_shm.cpp
    >
)

# local headers
target_include_directories(eROIL 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/address
        ${CMAKE_CURRENT_SOURCE_DIR}/src/broadcast
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/events
        ${CMAKE_CURRENT_SOURCE_DIR}/src/manager
        ${CMAKE_CURRENT_SOURCE_DIR}/src/net
        ${CMAKE_CURRENT_SOURCE_DIR}/src/router
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers

    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/eROIL
)

# build options
set_target_properties(eROIL PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# strict warnings only for non-system code
target_compile_options(eROIL PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wunused-parameter -Wunused-variable>
)

# external libs
target_link_libraries(eROIL PRIVATE
    $<$<PLATFORM_ID:Windows>:ws2_32>
)