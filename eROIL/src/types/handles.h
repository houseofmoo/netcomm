#pragma once
#include <memory>
#include "iosb.h"
#include "types.h"

namespace eroil {
  
    struct OpenSendData {
        int32_t label;
        uint8_t* buf;
        size_t buf_size;
        uint32_t buf_offset;
        sem_handle sem; // windows void*, linux sem_t*
        SendIosb* iosb;
        uint32_t num_iosb;
        size_t iosb_index;
    };

    struct SendHandle {
        handle_uid uid;
        OpenSendData data;

        SendHandle(int id, OpenSendData data) : uid(id), data(data) {}
    };

    struct OpenReceiveData{
        int32_t label;
        int32_t forward_label;  // the fuck this for?
        uint8_t* buf;
        size_t buf_size;
        size_t buf_slots;
        size_t buf_index;
        uint8_t* aux_buf;
        sem_handle sem; // windows void*, linux sem_t*
        ReceiveIosb* iosb;
        uint32_t num_iosb;
        size_t iosb_index;
        int signal_mode;
    };

    struct RecvTarget {
        OpenReceiveData data;

        RecvTarget(OpenReceiveData data) : data(data) {}
    };

    struct RecvHandle {
        handle_uid uid;
        std::shared_ptr<RecvTarget> target;

        RecvHandle(int id, OpenReceiveData data) : uid(id), target(std::make_shared<RecvTarget>(data)) {}
    };
}