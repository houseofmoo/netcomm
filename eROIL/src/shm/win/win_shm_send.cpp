#include "shm/shm.h"
#include <algorithm>
#include <cstring>
#include "eROIL/print.h"

namespace eroil::shm {
    ShmSend::ShmSend(const Label label, const size_t label_size) : Shm(label, label_size) {}

    ShmOpErr ShmSend::add_send_event(const NodeId to_id) {
        // check if event already exists
        auto exists = std::find_if(
            m_send_events.begin(),
            m_send_events.end(),
            [&](const evt::NamedEvent& e) {
                auto info = e.get_info();
                return info.label_id == m_label && info.destination_id == to_id;
            }
        );
        if (exists != m_send_events.end()) return ShmOpErr::DuplicateSendEvent;

        // attempt to open new event
        evt::NamedEvent evt(m_label, to_id);
        auto error = evt.open();
        if (error != evt::NamedEventErr::None) {
            using err = evt::NamedEventErr;
            switch (error) {
                case err::DoubleOpen: ERR_PRINT("Attempted to open existing send event"); break;
                case err::InvalidName: ERR_PRINT("Attempted to open send event with invalid name"); break;
                case err::OpenFailed: ERR_PRINT("Attempted to open send event but failed"); break;
                default: ERR_PRINT("Attempted to open send event but got unknown error"); break;
            }
            return ShmOpErr::AddSendEventError;
        }

        m_send_events.emplace_back(std::move(evt));
        return ShmOpErr::None;
    }
    
    bool ShmSend::has_send_event(const Label label, const NodeId to_id) const {
        if (label != m_label) return false;
        auto found_it = std::find_if(
            m_send_events.begin(),
            m_send_events.end(),
            [&](const evt::NamedEvent& e) {
                auto info = e.get_info();
                return info.destination_id == to_id;
            }
        );
        return found_it != m_send_events.end();
    }

    void ShmSend::remove_send_event(const NodeId to_id) {
        m_send_events.erase(
            std::remove_if(
                m_send_events.begin(),
                m_send_events.end(),
                [&](const evt::NamedEvent& e) {
                    auto info = e.get_info();
                    return info.label_id == m_label && info.destination_id == to_id;
                }
            ),
            m_send_events.end()
        );
    }

    ShmOpErr ShmSend::send(const void* buf, const size_t size) {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > m_label_size) return ShmOpErr::TooLarge;

        if (auto err = write(buf, size); err != ShmOpErr::None) {
            return err;
        }

        int error_count = 0;
        for (auto& ev : m_send_events) {
            auto ev_error = ev.post();
            if (ev_error != evt::NamedEventErr::None) {
                error_count += 1;
                using err = evt::NamedEventErr;
                switch (ev_error) {
                    case err::NotInitialized: ERR_PRINT("Attempted to signal uninitialized send event"); break;
                    case err::SignalFailed: ERR_PRINT("Attempted to signal send event and failed"); break;
                    default: ERR_PRINT("Attempted to signal send event but got unknown error"); break;
                }
            }
        }

        if (error_count > 0) {
            ERR_PRINT("Failed to signal ", error_count, " named send events");
        }
        return ShmOpErr::None;
    }
}