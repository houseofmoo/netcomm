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
        ShmMissing,
        SocketMissing,
        Failed,
    };

    class Dispatcher {
        private:
            static bool validate_recv_target(const OpenReceiveData& data);

        public:
            SendOpErr dispatch_send(const SendPlan& plan,
                                    const SendTargets& targets,
                                    const void* buf,
                                    size_t size) const;

           void  dispatch_recv_targets(Label label,
                                       size_t label_size,
                                       const void* buf,
                                       size_t size,
                                       const std::vector<std::shared_ptr<RecvTarget>>& targets) const;
    };
}
