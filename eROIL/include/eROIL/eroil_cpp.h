#pragma once
#include <cstdint>

// C++ include file
// either import this into a C++ project or eroil_c.h for a C project

//
// init
//
bool init_manager(int32_t id);
bool is_ready();
int32_t get_manager_id();
uint32_t get_roil_id();

//
// send label
//
void* open_send_label(
    int32_t label, 
    uint8_t* buf, 
    int32_t size_in_words,
    int32_t offset_mode,
    void* sem,
    void* iosb,
    int32_t num_iosb
);

void send_label(
    void* handle, 
    uint8_t* buf, 
    int32_t buf_size, 
    int32_t send_offset,
    int32_t recv_offset
);

void close_send(void* handle);

//
// recv label
//
void* open_recv_label(
    int32_t label, 
    uint8_t* buf, 
    int32_t size,
    int32_t num_slots,
    uint8_t* aux_buf,
    void* sem,
    void** iosb,
    int32_t num_iosb,
    int signal_mode
);
void close_recv_label(void* handle);
uint32_t recv_count(void* handle);
void recv_dismiss(void* handle, int32_t count);
void recv_idle(void* handle);
void recv_resume(void* handle);
void recv_reset(void* handle);
void recv_redirect(void* handle);

//
// queries
//
int32_t get_msg_label(void* iosb);
int32_t get_msg_status(void* iosb);
int32_t get_msg_size(void* iosb);
void* get_msg_address(void* iosb);
int32_t get_msg_offset(void* iosb);
void* get_msg_buffer(void* iosb);
int32_t get_msg_slot(void* iosb);

//
// time
//
void get_msg_timestamp(void* iosb, double* raw_time);
void get_current_time(void* time);
