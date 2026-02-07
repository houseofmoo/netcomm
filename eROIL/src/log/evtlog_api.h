#pragma once

#include "evtlog.h"
#include "evtrecord.h"
#include "macros.h"

namespace eroil {
    using elog_kind = eroil::evtlog::EventKind;
    using elog_sev = eroil::evtlog::Severity;
    using elog_cat = eroil::evtlog::Category;
    
    namespace evtlog {
        // call this once at startup to estimate tsc frequency.
        // used for time measurements in logs
        uint64_t estimate_tsc_hz() noexcept;

        namespace detail {
            struct slot32 {
                std::int32_t value;

                constexpr slot32() noexcept : value(0) {}
                constexpr slot32(std::int32_t x) noexcept : value(x) {}
                constexpr slot32(std::uint32_t x) noexcept : value(static_cast<std::int32_t>(x)) {}

                template <class E, std::enable_if_t<std::is_enum<E>::value, int> = 0>
                constexpr slot32(E e) noexcept
                : value(static_cast<slot32>(static_cast<std::underlying_type_t<E>>(e))) {}
            };
        }

        //
        // EXPOSED WRITE FUNCTIONS
        //
        inline void write_evtlog() noexcept {
            g_event_log.write_evtlog();
        }
        
        inline void write_evtlog(const std::string& directory) noexcept {
            g_event_log.write_evtlog(directory);
        }

        //
        // ACCESS LOGGING FUNCTIONS THROUGH THESE WRAPPERS
        // DO NOT CALL evtlog::g_event_log METHODS DIRECTLY
        //

        // log that includes timestamp + payload copy (payload copy is slow, do not use in hot paths)
        inline void info_pl(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Info>(kind, cat, a.value, b.value, c.value, payload, payload_size); 
        }

        inline void warn_pl(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Warning>(kind, cat, a.value, b.value, c.value, payload, payload_size); 
        }

        inline void error_pl(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Error>(kind, cat, a.value, b.value, c.value, payload, payload_size); 
        }

        inline void crit_pl(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Critical>(kind, cat, a.value, b.value, c.value, payload, payload_size); 
        }

        // logs that include a timestamp and no payload (in general this is the default case)
        inline void info(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot<elog_sev::Info>(kind, cat, a.value, b.value, c.value); 
        }

        inline void warn(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot<elog_sev::Warning>(kind, cat, a.value, b.value, c.value); 
        }

        inline void error(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot<elog_sev::Error>(kind, cat, a.value, b.value, c.value); 
        }

        inline void crit(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot<elog_sev::Critical>(kind, cat, a.value, b.value, c.value); 
        }

        // logs that have no timestamp or payload (for use in very hot paths if timestamps are not required)
        inline void info_bare(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Info>(kind, cat, a.value, b.value, c.value); 
        }

        inline void warn_bare(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Warning>(kind, cat, a.value, b.value, c.value); 
        }

        inline void error_bare(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Error>(kind, cat, a.value, b.value, c.value); 
        }

        inline void crit_bare(elog_kind kind, elog_cat cat, detail::slot32 a={}, detail::slot32 b={}, detail::slot32 c={}) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Critical>(kind, cat, a.value, b.value, c.value); 
        }
    }

    // use RAII to mark start/stops for you
    class EvtMark {
        private:
            elog_cat m_cat;
        public:
            explicit EvtMark(elog_cat cat) : m_cat(cat) {
                evtlog::info(elog_kind::Start, cat);
            }

            ~EvtMark() {
                evtlog::info(elog_kind::End, m_cat);
            }

            EROIL_NO_COPY(EvtMark)
            EROIL_NO_MOVE(EvtMark)
    };
}
