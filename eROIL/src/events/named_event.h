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
        WaitFailed,         // wait() failed (WAIT_FAILED, etc.)
        Timeout,            // Timed wait expired
    };

    struct NamedEventInfo {
        Label label_id;
        NodeId destination_id;
    };

    class NamedEvent {
        private:
            Label m_label_id;
            NodeId m_destination_id;
            sem_handle m_sem;

        public:
            NamedEvent(Label label, NodeId dest_node_id);
            ~NamedEvent();

            NamedEvent(NamedEvent&& other) noexcept;
            NamedEvent& operator=(NamedEvent&& other) noexcept;
            
            std::string name() const;
            NamedEventErr open();
            NamedEventErr post() const;
            NamedEventErr try_wait() const;
            NamedEventErr wait() const;
            NamedEventErr wait(uint32_t milliseconds) const;
            NamedEventInfo get_info() const { return { m_label_id, m_destination_id }; }
            void close();
    };
}