#include <eROIL/eroil_cpp.h>
#include <eROIL/print.h>
#include "types/handles.h"
#include "types/const_types.h"
#include "root.h"

// implementation for eroil_cpp.h
// calls into root.cpp

bool init_manager(std::int32_t id) {
    return eroil::init_manager(static_cast<eroil::NodeId>(id));
}

bool is_ready() {
    return eroil::is_ready();
}

std::int32_t get_manager_id() {
    return eroil::get_manager_id();
}

std::uint32_t get_roil_id() {
    return eroil::get_roil_id();
}

void* open_send_label(std::int32_t label, 
                      std::byte* buf, 
                      std::int32_t size_in_words,
                      std::int32_t offset_mode,
                      void* sem,
                      void* iosb,
                      std::int32_t num_iosb) {

    eroil::iosb::IoType io_type = eroil::iosb::IoType::SLOT;
    if (offset_mode == static_cast<int32_t>(eroil::iosb::IoType::OFFSET)) {
        io_type = eroil::iosb::IoType::OFFSET;
    }

    eroil::hndl::SendHandle* handle = eroil::open_send_label(
        static_cast<eroil::Label>(label),
        buf,
        size_in_words,
        io_type,
        static_cast<eroil::sem_handle>(sem),
        static_cast<eroil::iosb::SendIosb*>(iosb),
        static_cast<std::int32_t>(num_iosb)
    );

    return static_cast<void*>(handle);
}

void send_label(void* handle, 
                std::byte* buf, 
                std::int32_t buf_size, 
                std::int32_t send_offset,
                std::int32_t recv_offset) {

    eroil::send_label(
        static_cast<eroil::hndl::SendHandle*>(handle),
        buf,
        buf_size,
        send_offset,
        recv_offset
    );
}

void close_send_label(void* handle) {
    eroil::close_send_label(static_cast<eroil::hndl::SendHandle*>(handle));
}

void* open_recv_label(std::int32_t label, 
                      std::byte* buf, 
                      std::int32_t size,
                      std::int32_t num_slots,
                      std::byte* aux_buf,
                      void* sem,
                      void* iosb,
                      std::int32_t num_iosb,
                      std::int32_t signal_mode) {
   
    eroil::iosb::SignalMode smode = eroil::iosb::SignalMode::OVERWRITE;
    if (signal_mode == 1) {
        smode = eroil::iosb::SignalMode::BUFFER_FULL;
    } else if (signal_mode == 2) {
        smode = eroil::iosb::SignalMode::EVERY_MESSAGE;
    }

    return eroil::open_recv_label(
        static_cast<eroil::Label>(label),
        buf,
        size,
        num_slots,
        aux_buf,
        static_cast<eroil::sem_handle>(sem),
        static_cast<eroil::iosb::ReceiveIosb*>(iosb),
        num_iosb,
        smode
    );
 
}

void close_recv_label(void* handle) {
    eroil::close_recv_label(static_cast<eroil::hndl::RecvHandle*>(handle));
}

std::uint32_t recv_count(void* handle) {
    return eroil::recv_count(static_cast<eroil::hndl::RecvHandle*>(handle));
}

void recv_dismiss(void* handle, int32_t count) {
    return eroil::recv_dismiss(static_cast<eroil::hndl::RecvHandle*>(handle), count);
}

void recv_idle(void* handle) {
    if (handle == nullptr) return;
    // TODO: implement
    // stop recving labels
}

void recv_resume(void* handle) {
    if (handle == nullptr) return;
    // TODO: implement
    // resume recving labels
}

void recv_reset(void* handle) {
    eroil::recv_reset(static_cast<eroil::hndl::RecvHandle*>(handle));
}

void recv_redirect(void* handle) {
    eroil::recv_redirect(static_cast<eroil::hndl::RecvHandle*>(handle));
}

std::int32_t get_msg_label(void* iosb) {
    return eroil::get_msg_label(static_cast<eroil::iosb::Iosb*>(iosb));
}

std::int32_t get_msg_status(void* iosb) {
    return eroil::get_msg_status(static_cast<eroil::iosb::Iosb*>(iosb));
}

std::int32_t get_msg_size(void* iosb) {
    return eroil::get_msg_size(static_cast<eroil::iosb::Iosb*>(iosb));
}

void* get_msg_address(void* iosb) {
    return eroil::get_msg_address(static_cast<eroil::iosb::Iosb*>(iosb));
}

std::int32_t get_msg_offset(void* iosb) {
    return eroil::get_msg_offset(static_cast<eroil::iosb::Iosb*>(iosb));
}

void* get_msg_buffer(void* iosb) {
    return eroil::get_msg_buffer(static_cast<eroil::iosb::Iosb*>(iosb));
}

std::int32_t get_msg_slot(void* iosb) {
    return eroil::get_msg_slot(static_cast<eroil::iosb::Iosb*>(iosb));
}

void get_msg_timestamp(void* iosb, double* raw_time) {
    eroil::get_msg_timestamp(static_cast<eroil::iosb::Iosb*>(iosb), raw_time);
}

void get_current_time(void* time) {
    eroil::get_current_time(static_cast<eroil::iosb::RTOSTime*>(time));
}