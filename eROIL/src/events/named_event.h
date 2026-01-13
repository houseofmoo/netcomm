#pragma once

#include <string>
#include "types/types.h"

namespace eroil::evt {
    enum class NamedEventErr {
        None,               // Success
        DoubleOpen,         // Called open() twice on this named event
        NotInitialized,     // Event not created / already closed
        InvalidName,        // Name could not be constructed or rejected
        OpenFailed,         // OS failed to create/open named event
        SignalFailed,       // post()/SetEvent failed
        Timeout,            // Timed wait expired
        WouldBlock,
        SysError,           // wait() failed (WAIT_FAILED, etc.)
    };

    struct NamedEventInfo {
        Label label_id;
        NodeId src_id;
        NodeId dst_id;
    };

    class NamedEvent {
        private:
            Label m_label_id;
            NodeId m_src_id;
            NodeId m_dst_id;
            sem_handle m_sem;

        public:
            NamedEvent(Label label, NodeId src_id, NodeId dst_id);
            ~NamedEvent();

            // do not copy
            NamedEvent(const NamedEvent&) = delete;
            NamedEvent& operator=(const NamedEvent&) = delete;

            // allow move
            NamedEvent(NamedEvent&& other) noexcept;
            NamedEvent& operator=(NamedEvent&& other) noexcept;
            
            std::string name() const;
            NamedEventInfo get_info() const { return { m_label_id, m_src_id, m_dst_id }; }
            NamedEventErr post() const;
            NamedEventErr try_wait() const;
            NamedEventErr wait(uint32_t milliseconds = 0) const;
            void close();

        private: 
            NamedEventErr open();
    };
}