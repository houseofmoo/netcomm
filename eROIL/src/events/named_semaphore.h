#pragma once

#include <string>
#include "types/const_types.h"
#include "macros.h"

namespace eroil::evt {
    enum class NamedSemErr {
        None = 0,           // Success
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

    enum class NamedSemOp {
        Open,
        Post,
        TryWait,
        Wait
    };
    
    struct NamedSemResult {
        NamedSemErr code;
        NamedSemOp op;

        bool ok() const noexcept {
            return code == NamedSemErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case NamedSemErr::None: return "None";
                case NamedSemErr::DoubleOpen: return "DoubleOpen";
                case NamedSemErr::NotInitialized: return "NotInitialized";
                case NamedSemErr::InvalidName: return "InvalidName";
                case NamedSemErr::OpenFailed: return "OpenFailed";
                case NamedSemErr::SignalFailed: return "SignalFailed";
                case NamedSemErr::Timeout: return "Timeout";
                case NamedSemErr::WouldBlock: return "WouldBlock";
                case NamedSemErr::MaxCount: return "MaxCount";
                case NamedSemErr::SysError: return "SysError";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case NamedSemOp::Open: return "Open";
                case NamedSemOp::Post: return "Post";
                case NamedSemOp::TryWait: return "TryWait";
                case NamedSemOp::Wait: return "Wait";
                default: return "Unknown - op is undefined";
            }
        }
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
            NO_DISCARD NamedSemResult post() const;
            NO_DISCARD NamedSemResult try_wait() const;
            NO_DISCARD NamedSemResult wait(uint32_t milliseconds = 0) const;
            void close();

        private: 
            NO_DISCARD NamedSemResult open();
    };
}