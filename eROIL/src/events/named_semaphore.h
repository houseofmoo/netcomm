#pragma once

#include <string>
#include "types/const_types.h"
#include "types/macros.h"

namespace eroil::evt {
    enum class NamedSemErr {
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

    // counting semaphores
    class NamedSemaphore {
        private:
            NodeId m_dst_id; 
            sem_handle m_sem;

        public:
            NamedSemaphore(NodeId id);
            ~NamedSemaphore();

            EROIL_NO_COPY(NamedSemaphore)
            EROIL_DECL_MOVE(NamedSemaphore)
            
            std::string name() const;
            NodeId get_dst_id() const { return m_dst_id; }
            NamedSemErr post() const;
            NamedSemErr try_wait() const;
            NamedSemErr wait(uint32_t milliseconds = 0) const;
            void close();

        private: 
            NamedSemErr open();
    };
}