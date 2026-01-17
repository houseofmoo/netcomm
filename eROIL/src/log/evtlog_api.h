#include "evtlog.h"
#include "evtrecord.h"

namespace eroil {
    using elog_kind = eroil::evtlog::EventKind;
    using elog_sev = eroil::evtlog::Severity;
    using elog_cat = eroil::evtlog::Category;
    
    namespace evtlog {
        // call this once at startup to estimate tsc frequency.
        // used for time measurements in logs
        uint64_t estimate_tsc_hz() noexcept;

        //
        // ACCESS LOGGING FUNCTIONS THROUGH THESE WRAPPERS
        // DO NOT CALL evtlog::g_event_log METHODS DIRECTLY
        //

        // full logs with payload copy
        inline void debug_pl(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Debug>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void info_pl(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Info>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void warn_pl(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Warning>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void error_pl(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Error>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void crit_pl(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            evtlog::detail::log_payload<elog_sev::Critical>(kind, cat, a, b, c, payload, payload_size); 
        }

        // logs without payload copy
        inline void debug(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot<elog_sev::Debug>(kind, cat, a, b, c); 
        }

        inline void info(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot<elog_sev::Info>(kind, cat, a, b, c); 
        }

        inline void warn(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot<elog_sev::Warning>(kind, cat, a, b, c); 
        }

        inline void error(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot<elog_sev::Error>(kind, cat, a, b, c); 
        }

        inline void crit(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot<elog_sev::Critical>(kind, cat, a, b, c); 
        }

        // logs without payload copy or time stamp
        inline void debug_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Debug>(kind, cat, a, b, c); 
        }

        inline void info_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Info>(kind, cat, a, b, c); 
        }

        inline void warn_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Warning>(kind, cat, a, b, c); 
        }

        inline void error_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Error>(kind, cat, a, b, c); 
        }

        inline void crit_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            evtlog::detail::log_hot_no_time<elog_sev::Critical>(kind, cat, a, b, c); 
        }
    }
}
