#pragma once
#include <memory>
#include <mutex>
#include "iosb.h"
#include "const_types.h"

namespace eroil::hndl {
  
    struct OpenSendData {
        int32_t label;
        std::byte* buf;
        size_t buf_size;
        bool is_offset;
        sem_handle sem;
        iosb::SendIosb* iosb;
        uint32_t num_iosb;
        size_t iosb_index;
    };

    struct SendHandle {
        std::mutex mtx;
        handle_uid uid;
        OpenSendData data;
        SendHandle(int id, OpenSendData d) : uid(id), data(d) {}
    };

    struct OpenReceiveData{
        int32_t label;
        std::byte* buf;
        size_t buf_size;
        uint32_t buf_slots;
        size_t buf_index;
        std::byte* aux_buf;
        sem_handle sem;
        iosb::ReceiveIosb* iosb;
        uint32_t num_iosb;
        size_t iosb_index;
        iosb::SignalMode signal_mode;
        uint32_t recv_count;
    };

    struct RecvHandle {
        std::mutex mtx;
        handle_uid uid;
        bool is_idle;
        OpenReceiveData data;
        RecvHandle(int id, OpenReceiveData d) : uid(id), is_idle(false), data(d) {}
    };
}