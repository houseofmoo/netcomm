#pragma once
#include <cstddef>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <vector>

#include "types/types.h"
#include "types/handles.h"

#include "route_table.h"
#include "transport_registry.h"

namespace eroil {
    enum class SendOpErr {
        None,
        RouteNotFound,
        SizeMismatch,
        SizeTooLarge,
        ShmMissing,
        SocketMissing,
        UnknownHandle,
        NoPublishers,
        IncorrectPublisher,
        Failed
    };

    struct SendResult {
        SendOpErr send_err;
        shm::ShmOpErr shm_err;
        std::unordered_map<NodeId, sock::SockResult> sock_err;
    };

    struct SendTargets {
        NodeId source_id;
        Label label;
        std::shared_ptr<hndl::OpenSendData> publisher;
        bool has_local;
        std::vector<std::shared_ptr<shm::ShmSend>> shm;
        bool has_remote;
        std::vector<std::shared_ptr<sock::TCPClient>> sockets;
    };

    struct RecvTargets {
        Label label;
        std::vector<std::shared_ptr<hndl::OpenReceiveData>> subscribers;
    };

    class Dispatcher {
        public:
            SendResult dispatch_send_targets(const SendTargets& targets, io::SendBuf send_buf) const;

           void  dispatch_recv_targets(const RecvTargets& targets,
                                       const std::byte* buf,
                                       const size_t size,
                                       const size_t recv_offset) const;
    };
}
