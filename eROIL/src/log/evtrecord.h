#pragma once
#include <cstdint>
#include <atomic>

namespace eroil::evtlog {
        enum class EventKind : std::uint16_t {
        None = 0,

        // send pipeline
        Send_Start,
        Send_Failed,
        Send_RouteLookup,
        Send_Shm_Write,
        Send_Socket_Write,
        Send_End,

        // recv pipeline
        Recv_Start,
        Recv_Shm_Read,
        Recv_Socket_Read,
        Recv_End,

        // Workers / threads
        SendWorker_Start,
        SendWorker_Warning,
        SendWorker_Error,
        SendWorker_End,
        SendWorker_Exits,
        
        ShmRecvWorker_Start,
        ShmRecvWorker_Warning,
        ShmRecvWorker_Error,
        ShmRecvWorker_End,
        ShmRecvWorker_Exit,

        SocketRecvWorker_Start,
        SocketRecvWorker_Warning,
        SocketRecvWorker_Error,
        SocketRecvWorker_End,
        SocketRecvWorker_Exit,

        // subsribers / publishers
        AddLocalSendSubscriber,
        AddRemoteSendSubscriber,
        RemoveLocalSendSubscriber,
        RemoveRemoteSendSubscriber,
        AddLocalRecvPublisher,
        AddRemoteRecvPublisher,
        RemoveLocalRecvPublisher,
        RemoveRemoteRecvPublisher,

        // monitor
        SocketMonitor_Start,
        MissingSocket,
        DeadSocket_Found,
        Connect_Start,
        Connect_Failed,
        Connect_Send_Failed,
        Connect_Success,
        SocketMonitor_End,

        // TCP Server
        TCPServer_Start,
        StartFailed,
        AcceptFailed,
        ConnectionFailed,
        InvalidHeader,
        NewConnection,

        Error,
    };

    enum class Severity : std::uint8_t { Debug, Info, Warning, Error, Critical };
    
    enum class Category : std::uint8_t { 
        General, 
        Router, 
        Shm, 
        Socket, 
        Worker, 
        Broadcast, 
        SocketMonitor, 
        TCPServer 
    };

    // 20 bytes payload keeps record at 48 bytes (on typical packing).
    static constexpr size_t PAYLOAD_BYTES = 20;

    struct EventRecord {
        std::uint64_t tick = 0;
        //std::uint32_t thread_id;
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        std::uint32_t c = 0;
        std::uint16_t kind = 0;
        std::uint8_t severity = 0;
        std::uint8_t category = 0;
        std::uint8_t payload[PAYLOAD_BYTES];
        std::atomic<std::uint32_t> commit_seq{0};
    };
    static_assert(sizeof(EventRecord) == 48, "check packing/size for EventRecord, expected 48 bytes");
}
