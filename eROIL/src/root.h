#pragma once
#include <string>
#include "types/const_types.h"
#include "types/handles.h"
#include "types/iosb.h"

namespace eroil {
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
    hndl::SendHandle* open_send_label(
        Label label, 
        std::byte* buf, 
        size_t size,
        iosb::IoType io_type,
        sem_handle sem,
        iosb::SendIosb* iosb,
        uint32_t num_iosb
    );

    void send_label(
        hndl::SendHandle* handle, 
        std::byte* buf, 
        size_t buf_size, 
        size_t send_offset,
        size_t recv_offset
    );

    void close_send_label(hndl::SendHandle* handle);
    
    //
    // recv label
    //
    hndl::RecvHandle* open_recv_label(
        Label label, 
        std::byte* buf, 
        size_t size,
        uint32_t num_slots,
        std::byte* aux_buf,
        sem_handle sem,
        iosb::ReceiveIosb* iosb,
        uint32_t num_iosb,
        iosb::SignalMode signal_mode
    );
    void close_recv_label(hndl::RecvHandle* handle);
    uint32_t recv_count(hndl::RecvHandle* handle);
    void recv_dismiss(hndl::RecvHandle* handle, uint32_t count);
    void recv_idle(hndl::RecvHandle* handle);
    void recv_resume(hndl::RecvHandle* handle);
    void recv_reset(hndl::RecvHandle* handle);
    void recv_redirect(hndl::RecvHandle* handle);

    //
    // queries
    //
    int32_t get_msg_label(iosb::Iosb* iosb);
    int32_t get_msg_status(iosb::Iosb* iosb);
    int32_t get_msg_size(iosb::Iosb* iosb);
    void* get_msg_address(iosb::Iosb* iosb);
    int32_t get_msg_offset(iosb::Iosb* iosb);
    void* get_msg_buffer(iosb::Iosb* iosb);
    int32_t get_msg_slot(iosb::Iosb* iosb);

    //
    // time
    //
    void get_msg_timestamp(iosb::Iosb* iosb, double* raw_time);
    void get_current_time(iosb::RTOSTime* time);

    //
    // logs
    //
    void write_event_log() noexcept;
    void write_event_log(const std::string& directory) noexcept;
}