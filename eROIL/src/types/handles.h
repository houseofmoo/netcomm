#pragma once
#include <memory>
#include "iosb.h"
#include "types.h"

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
        handle_uid uid;
        std::shared_ptr<OpenSendData> data;

        SendHandle(int id, OpenSendData data) 
            : uid(id), data(std::make_unique<OpenSendData>(data)) {}
    };

    struct OpenReceiveData{
        int32_t label;
        int32_t forward_label;  // this exists for IMC stuff in NAE, probably delete it
        std::byte* buf;
        size_t buf_size;
        uint32_t buf_slots;
        size_t buf_index; // TODO: maybe make atomic?
        std::byte* aux_buf;
        sem_handle sem;
        iosb::ReceiveIosb* iosb;
        uint32_t num_iosb;
        size_t iosb_index;
        iosb::SignalMode signal_mode;
        uint32_t recv_count; // TODO: maybe make atomic?
    };

    struct RecvHandle {
        handle_uid uid;
        std::shared_ptr<OpenReceiveData> data;
        RecvHandle(int id, OpenReceiveData data) 
            : uid(id), data(std::make_shared<OpenReceiveData>(data)) {}
    };
}