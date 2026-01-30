#include "root.h"
#include <mutex>
#include "config/config.h"
#include "address/address.h"
#include "manager/manager.h"
#include "rtos.h"

namespace eroil {
    static std::unique_ptr<eroil::Manager> manager;
    static cfg::ManagerConfig config{};
    static bool initialized = false;

    bool init_manager(int32_t id) {
        print::set_id(id);

        // read etc/manager.cfg
        config = cfg::get_manager_cfg(id);

        // read etc/peer_ips.cfg
        if (!addr::init_address_book(id)) return false;

        // set up address book
        switch (config.mode) {
            case cfg::ManagerMode::Normal: { 
                break; 
            }
            case cfg::ManagerMode::TestMode_Local_ShmOnly: {
                addr::all_shm_address_book();
                break;
            }
            case cfg::ManagerMode::TestMode_Local_SocketOnly: {
                addr::all_socket_address_book();
                break;
            }
            case cfg::ManagerMode::TestMode_Sim_Network: {
                addr::test_network_address_book();
                break;
            }
        }

        // initialize manager
        manager = std::make_unique<Manager>(config);
        initialized = manager->init();
        return initialized;
    }

    bool is_ready() {
        return initialized && manager != nullptr;
    }

    int32_t get_manager_id() {
        return config.id;
    }

    uint32_t get_roil_id() {
        // this is a dedicated list of ID's provided by roil
        struct NAE_Node {
            int32_t iNodeId;
            int32_t iModuleId;
            int32_t iCoreId;
            uint32_t iRoilNodeId; 
        };

        static const NAE_Node nodes[20] = {
                        // iNodeId, iModuleId, iCoreId, iRoilNodeId
            /* sp00 */ {   0,       0,         0,       0x73703030 },
            /* sp01 */ {   1,       0,         1,       0x73703031 },
            /* sp02 */ {   2,       0,         2,       0x73703032 },
            /* sp03 */ {   3,       0,         3,       0x73703033 },
            /* sp04 */ {   4,       1,         0,       0x73703034 },
            /* sp05 */ {   5,       1,         1,       0x73703035 },
            /* sp06 */ {   6,       1,         2,       0x73703036 },
            /* sp07 */ {   7,       1,         3,       0x73703037 },
            /* sp08 */ {   8,       2,         0,       0x73703038 },
            /* sp09 */ {   9,       2,         1,       0x73703039 },
            /* sp10 */ {   10,      2,         2,       0x73703130 },
            /* sp11 */ {   11,      2,         3,       0x73703131 },
            /* sp12 */ {   12,      3,         0,       0x73703132 },
            /* sp13 */ {   13,      3,         1,       0x73703133 },
            /* sp14 */ {   14,      3,         2,       0x73703134 },
            /* sp15 */ {   15,      3,         3,       0x73703135 },
            /* bp00 */ {   16,      4,         0,       0x62703030 },
            /* bp01 */ {   17,      4,         1,       0x62703031 },
            /* bp02 */ {   18,      4,         2,       0x62703032 },
            /* dp01 */ {   19,      4,         3,       0x64703031 },
        };

        return nodes[config.id].iRoilNodeId;
    }

    hndl::SendHandle* open_send_label(Label label, 
                                      std::byte* buf, 
                                      int32_t size,
                                      iosb::IoType io_type,
                                      sem_handle sem,
                                      iosb::SendIosb* iosb,
                                      int32_t num_iosb) {

        if (!is_ready()) return nullptr;
        if (label <= INVALID_LABEL) return nullptr;
        if (buf == nullptr) return nullptr;
        if (size <= 0) return nullptr;
        if (num_iosb < 0) num_iosb = 0;
        
        hndl::OpenSendData data;
        data.label = label;
        data.buf = buf;
        data.buf_size = static_cast<size_t>(size);
        data.is_offset = io_type == iosb::IoType::OFFSET; 
        data.sem = sem;
        data.iosb = iosb;
        data.num_iosb = static_cast<uint32_t>(num_iosb);
        data.iosb_index = 0;

        return manager->open_send(data);
    }

    void send_label(hndl::SendHandle* handle, 
                    std::byte* buf, 
                    int32_t buf_size, 
                    int32_t send_offset,
                    int32_t recv_offset) {
        
        if (!is_ready()) return;
        if (handle == nullptr) return;
        if (buf_size < 0) buf_size = 0;
        if (send_offset < 0) send_offset = 0;
        if (recv_offset < 0) recv_offset = 0;

        manager->send_label(
            handle, 
            buf, 
            static_cast<size_t>(buf_size), 
            static_cast<size_t>(send_offset), 
            static_cast<size_t>(recv_offset)
        );
    }

    void close_send_label(hndl::SendHandle* handle) {
        if (!is_ready()) return;
        if (handle == nullptr) return;

        manager->close_send(handle);
    }

    hndl::RecvHandle* open_recv_label(Label label, 
                                      std::byte* buf, 
                                      int32_t size,
                                      int32_t num_slots,
                                      std::byte* aux_buf,
                                      sem_handle sem,
                                      iosb::ReceiveIosb* iosb,
                                      int32_t num_iosb,
                                      iosb::SignalMode signal_mode) {

        if (!is_ready()) return nullptr;
        if (label <= INVALID_LABEL) return nullptr;
        if (buf == nullptr) return nullptr;
        if (size <= 0) return nullptr;
        if (num_slots <= 0) return nullptr;
        if (num_iosb < 0) num_iosb = 0;

        eroil::hndl::OpenReceiveData data;
        data.label = label;
        data.buf = buf;
        data.buf_size = static_cast<size_t>(size);
        data.buf_slots = static_cast<uint32_t>(num_slots);
        data.buf_index = 0;
        data.aux_buf = aux_buf;
        data.sem = sem;
        data.iosb = iosb;
        data.num_iosb = static_cast<uint32_t>(num_iosb);
        data.iosb_index = 0;
        data.signal_mode = signal_mode;
        data.recv_count = 0;
        
        return manager->open_recv(data);
    }

    void close_recv_label(hndl::RecvHandle* handle) {
        if (!is_ready()) return;
        if (handle == nullptr) return;

        manager->close_recv(handle);
    }

    uint32_t recv_count(hndl::RecvHandle* handle) {
        if (handle == nullptr) return 0;
        std::lock_guard lock(handle->mtx);
        return handle->data.recv_count;
    }

    void recv_dismiss(hndl::RecvHandle* handle, int32_t count) {
        if (handle == nullptr) return;
        if (count < 0) return;

        std::lock_guard lock(handle->mtx);
        if (static_cast<uint32_t>(count) > handle->data.recv_count) {
            handle->data.recv_count = 0;
        } else {
            handle->data.recv_count -= static_cast<uint32_t>(count);
        }
    }

    void recv_idle(hndl::RecvHandle* handle) {
        if (handle == nullptr) return;
        std::lock_guard lock(handle->mtx);
        handle->is_idle = true;
    }

    void recv_resume(hndl::RecvHandle* handle) {
        if (handle == nullptr) return;
        std::lock_guard lock(handle->mtx);
        handle->is_idle = false;
    }

    void recv_reset(hndl::RecvHandle* handle) {
        if (handle == nullptr) return;
        std::lock_guard lock(handle->mtx);
        handle->data.recv_count = 0;
        handle->data.buf_index = 0;
    }

    void recv_redirect(hndl::RecvHandle* handle) {
        if (handle == nullptr) return;
        if (handle->data.buf == nullptr) return;
        if (handle->data.aux_buf == nullptr) return;

        std::lock_guard lock(handle->mtx);
        std::byte* buf = handle->data.buf;
        handle->data.buf = handle->data.aux_buf;
        handle->data.aux_buf = buf;
        handle->data.buf_index = 0;
        handle->data.recv_count = 0; // i think we reset this
    }

    int32_t get_msg_label(iosb::Iosb* iosb) {
        if (iosb == nullptr) return 0;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            return siosb->FC_Header.Destination_ID;
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return riosb->FC_Header.Destination_ID;
        }
    }

    int32_t get_msg_status(iosb::Iosb* iosb) {
        if (iosb == nullptr) return 0;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            return siosb->Status;
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return riosb->Status;
        }
    }
    
    int32_t get_msg_size(iosb::Iosb* iosb) {
        if (iosb == nullptr) return 0;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            return siosb->MsgSize;
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return riosb->MsgSize;
        }
    }
    
    void* get_msg_address(iosb::Iosb* iosb) {
        if (iosb == nullptr) return nullptr;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            return static_cast<void*>(siosb->pMsgAddr);
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return static_cast<void*>(riosb->pMsgAddr);
        }
    }
    
    int32_t get_msg_offset(iosb::Iosb* iosb) {
        if (iosb == nullptr) return 0;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::RECEIVE)) {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return riosb->FC_Header.Parameter;
        }
        return 0;
    }
    
    void* get_msg_buffer(iosb::Iosb* iosb) {
        if (iosb == nullptr) return nullptr;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            return static_cast<void*>(siosb->pMsgAddr);
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return static_cast<void*>(riosb->pMsgAddr);
        }
    }
    
    int32_t get_msg_slot(iosb::Iosb* iosb) {
        if (iosb == nullptr) return 0;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::RECEIVE)) {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            return riosb->Messaage_Slot;
        }
        return 0;
    }
 
    void get_msg_timestamp(iosb::Iosb* iosb, double* raw_time) {
        if (iosb == nullptr) return;

        constexpr uint32_t USEC_PER_SEC = 1000000;
        constexpr uint32_t NSEC_PER_USEC = 1000;

        if (iosb->Reserve2 == static_cast<int>(iosb::RoilAction::SEND)) {
            auto siosb = reinterpret_cast<iosb::SendIosb*>(iosb);
            *raw_time = static_cast<double>(siosb->TimeStamp.uiMsb * USEC_PER_SEC) + 
                        static_cast<double>(siosb->TimeStamp.uiLsb / NSEC_PER_USEC);
        } else {
            auto riosb = reinterpret_cast<iosb::ReceiveIosb*>(iosb);
            *raw_time = static_cast<double>(riosb->TimeStamp.uiMsb * USEC_PER_SEC) + 
                        static_cast<double>(riosb->TimeStamp.uiLsb / NSEC_PER_USEC);
        }
    }

    void get_current_time(iosb::RTOSTime* time) {
        eroil::iosb::RTOSTime curr_time = eroil::RTOS_Current_Time_Raw();
        time->uiMsb = curr_time.uiMsb;
        time->uiLsb = curr_time.uiLsb;
    }
}
