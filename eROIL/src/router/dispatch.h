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
        Label label;
        std::shared_ptr<OpenSendData> publisher;
        bool has_local;
        std::shared_ptr<shm::Shm> shm;
        std::vector<std::shared_ptr<evt::NamedEvent>> shm_signals;
        bool has_remote;
        std::vector<std::shared_ptr<sock::TCPClient>> sockets;
    };

    struct RecvTargets {
        Label label;
        std::vector<std::shared_ptr<OpenReceiveData>> subscribers;
    };

    class Dispatcher {
        public:
            SendResult dispatch_send_targets(const SendTargets& targets, SendBuf send_buf) const;

           void  dispatch_recv_targets(const RecvTargets& targets,
                                       const void* buf,
                                       size_t size) const;
    };
}
