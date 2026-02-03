#pragma once
#include <cstdint>
#include <atomic>

namespace eroil::evtlog {
    enum class EventKind : std::uint16_t {
        None = 0,

        // manager
        NoSelfId,
        NoSockContext,
        NoComms,
        NoBroadcast,

        // generic
        Start,
        Failed,
        End,
        Exit,
        Send,
        SendFailed,
        Recv,
        RecvFailed,
        WaitError,
        RecvError,
        UnknownError,
        UnhandledError,

        // shared
        InvalidHeader,
        InvalidFlags,

        // send/recv
        MalformedRecv,
        InvalidDataSize,
        DataDistributed,
        PublishTimeout,
        BlockNotInitialized,
        BlockCorruption,

        // subsribers / publishers
        AddLocalSendSubscriber,
        AddRemoteSendSubscriber,
        RemoveLocalSendSubscriber,
        RemoveRemoteSendSubscriber,
        DuplicateLabel,

        // socket
        Connect,
        Ping,
        StartFailed,
        AcceptFailed,
        ConnectionFailed,
        NewConnection,
        Reconnected,
        DeadSocketFound,
    };

    enum class Severity : std::uint8_t { Info, Warning, Error, Critical };
    
    enum class Category : std::uint8_t { 
        General, 
        Manager,
        Router, 
        Shm, 
        Socket, 
        Worker,
        SendWorker,
        ShmRecvWorker,
        SocketRecvWorker,
        Broadcast, 
        SocketMonitor, 
        TCPServer 
    };

    // 20 bytes payload keeps record at 48 bytes (on typical packing).
    static constexpr std::size_t PAYLOAD_BYTES = 20;

    enum : std::uint16_t {
        HAS_TICK = 1 << 0,
        HAS_PAYLOAD = 1 << 1,
    };

    struct EventRecord {
        std::uint64_t tick = 0;
        //std::uint32_t thread_id;
        std::int32_t a = 0;
        std::int32_t b = 0;
        std::int32_t c = 0;
        std::uint16_t kind = 0;
        std::uint8_t severity = 0;
        std::uint8_t category = 0;
        std::uint16_t flags = 0;
        std::uint16_t payload_len = 0;
        std::uint8_t payload[PAYLOAD_BYTES];
        std::atomic<std::uint32_t> commit_seq{0};
    };
    static_assert(sizeof(EventRecord) == 56, "check packing/size for EventRecord, expected 56 bytes");

    struct EventRecordSnapshot {
        std::uint64_t tick;
        //std::uint32_t thread_id;
        std::int32_t a;
        std::int32_t b;
        std::int32_t c;
        std::uint16_t kind;
        std::uint8_t severity;
        std::uint8_t category;
        std::uint16_t flags;
        std::uint16_t payload_len;
        std::uint8_t payload[PAYLOAD_BYTES];
    };
    static_assert(offsetof(EventRecord, commit_seq) == sizeof(EventRecordSnapshot), "Snapshot must match EventRecord up to commit_seq");
    static_assert(std::is_trivially_copyable_v<EventRecordSnapshot>, "Snapshot must be trivially copyable");
}
