#pragma once

#include <string>
#include "types/const_types.h"
#include "types/macros.h"

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
        MaxCount,
        SysError,           // wait() failed (WAIT_FAILED, etc.)
    };

    class NamedEvent {
        private:
            NodeId m_dst_id; 
            sem_handle m_sem;

        public:
            NamedEvent(NodeId id);
            ~NamedEvent();

            EROIL_NO_COPY(NamedEvent)
            EROIL_DECL_MOVE(NamedEvent)
            
            std::string name() const;
            NodeId get_dst_id() const { return m_dst_id; }
            NamedEventErr post() const;
            NamedEventErr try_wait() const;
            NamedEventErr wait(uint32_t milliseconds = 0) const;
            void close();

        private: 
            NamedEventErr open();
    };
}