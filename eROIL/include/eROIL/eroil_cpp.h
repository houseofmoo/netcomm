#pragma once
#include <cstdint>
#include <cstddef>

// C++ include file
// either import this into a C++ project or eroil_c.h for a C project

//
// init
//
bool init_manager(std::int32_t id);
bool is_ready();
std::int32_t get_manager_id();
std::uint32_t get_roil_id();

//
// send label
//
void* open_send_label(
    std::int32_t label, 
    std::byte* buf, 
    std::int32_t size_in_words,
    std::int32_t offset_mode,
    void* sem,
    void* iosb,
    std::int32_t num_iosb
);

void send_label(
    void* handle, 
    std::byte* buf, 
    std::int32_t buf_size, 
    std::int32_t send_offset,
    std::int32_t recv_offset
);

void close_send(void* handle);

//
// recv label
//
void* open_recv_label(
    std::int32_t label, 
    std::byte* buf, 
    std::int32_t size,
    std::int32_t num_slots,
    std::byte* aux_buf,
    void* sem,
    void* iosb,
    std::int32_t num_iosb,
    std::int32_t signal_mode
);
void close_recv_label(void* handle);
std::uint32_t recv_count(void* handle);
void recv_dismiss(void* handle, std::int32_t count);
void recv_idle(void* handle);
void recv_resume(void* handle);
void recv_reset(void* handle);
void recv_redirect(void* handle);

//
// queries
//
std::int32_t get_msg_label(void* iosb);
std::int32_t get_msg_status(void* iosb);
std::int32_t get_msg_size(void* iosb);
void* get_msg_address(void* iosb);
std::int32_t get_msg_offset(void* iosb);
void* get_msg_buffer(void* iosb);
std::int32_t get_msg_slot(void* iosb);

//
// time
//
void get_msg_timestamp(void* iosb, double* raw_time);
void get_current_time(void* time);
