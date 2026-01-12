// #include "shm/shm.h"
// #include <algorithm>
// #include <cstring>
// #include "eROIL/print.h"

// namespace eroil::shm {
//     ShmRecv::ShmRecv(Label label, const size_t label_size) 
//         : Shm(label, label_size), m_recv_event(label, 0) {}

//     ShmOpErr ShmRecv::set_recv_event(NodeId my_id) {
//         m_recv_event.close(); // close the temp event we set up on construction

//         m_recv_event = evt::NamedEvent(m_label, my_id);
//         auto error = m_recv_event.open();
//         if (error != evt::NamedEventErr::None) {
//             using err = evt::NamedEventErr;
//             switch (error) {
//                 case err::DoubleOpen: ERR_PRINT("Attempted to open existing receive event"); break;
//                 case err::InvalidName: ERR_PRINT("Attempted to open receive event with invalid name"); break;
//                 case err::OpenFailed: ERR_PRINT("Attempted to open receive event but failed"); break;
//                 default: ERR_PRINT("Attempted to open receive event but got unknown error"); break;
//             }
//             return ShmOpErr::SetRecvEventError;
//         }

//         return ShmOpErr::None;
//     }

//     bool ShmRecv::has_recv_event(const Label label, const NodeId my_id) const {
//         if (label != m_label) return false;
//         return m_recv_event.get_info().destination_id == my_id;
//     }

//     ShmOpErr ShmRecv::recv(void* buf, const size_t size) {
//         if (!is_valid()) return ShmOpErr::NotOpen;
//         if (size > m_label_size) return ShmOpErr::TooLarge;

//         // block until signaled
//         auto error = m_recv_event.wait();
//         if (error != evt::NamedEventErr::None) {
//             using err = evt::NamedEventErr;
//             switch (error) {
//                 case err::NotInitialized: ERR_PRINT("Attempted to wait on uninitialized event"); break;
//                 case err::WaitFailed: ERR_PRINT("Attempted to wait on event and failed ???"); break;
//                 default: ERR_PRINT("Attempted to wait on event but got unknown error"); break;
//             }
//             return ShmOpErr::RecvFailed;
//         }

//         if (auto err = read(buf, size); err != ShmOpErr::None) {
//             return err;
//         }
//         return ShmOpErr::None;
//     }

//     ShmOpErr ShmRecv::recv_nonblocking(void* buf, const size_t size) {
//         if (!is_valid()) return ShmOpErr::NotOpen;
//         if (size > m_label_size) return ShmOpErr::TooLarge;

//         // if no data, return
//         auto error = m_recv_event.try_wait();
//         if (error != evt::NamedEventErr::None) {
//             using err = evt::NamedEventErr;
//             switch (error) {
//                 case err::NotInitialized: ERR_PRINT("Attempted to wait on uninitialized event"); break;
//                 case err::WaitFailed: ERR_PRINT("Attempted to wait on event and failed ???"); break;
//                 case err::Timeout: return ShmOpErr::WouldBlock; // not a failure, just no data
//                 default: ERR_PRINT("Attempted to wait on event but got unknown error"); break;
//             }
//             return ShmOpErr::RecvFailed;
//         }
   
//         if (auto err = read(buf, size); err != ShmOpErr::None) {
//             return err;
//         }
        
//         return ShmOpErr::None;
//     }

//     void ShmRecv::interrupt_wait() {
//         // since a recv event is unique to us for a given label
//         // we can safely post this event which will only wake up
//         // ourselves
//         m_recv_event.post(); 
//     }
// }