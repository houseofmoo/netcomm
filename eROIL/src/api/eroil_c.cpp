#include <string>
#include <eROIL/eroil_c.h>
#include <eROIL/print.h>
#include "types/handles.h"
#include "types/const_types.h"
#include "root.h"

// implementation for eroil_c.h
// calls into root.cpp

int NAE_Init(int /*iModuleId*/, int /*iProgramIDOffset*/, int /*iManager_CPU_ID*/, int /*iMaxNumCpus*/, int iNodeId) {
    bool success = eroil::init_manager(static_cast<int32_t>(iNodeId));
    return success ? 1 : 0;
}

int NAE_Is_Agent_Up(int /*iNodeId*/) {
    return eroil::is_ready() ? 1 : 0;
}

int NAE_Get_Node_ID() {
    return static_cast<int>(eroil::get_manager_id());
}

int NAE_Get_ROIL_Node_ID() {
    return static_cast<int>(eroil::get_roil_id());
}

void* NAE_Open_Send_Label(int iLabel,
                          void* pSendBuffer,
                          int iSizeInWords,
                          int iOffsetMode,
                          void* iSem,
                          void* pIosb,
                          int iNumIosbs) {

    // convert to expected types
    eroil::Label label = static_cast<eroil::Label>(iLabel);
    std::byte* buf = static_cast<std::byte*>(pSendBuffer);

    size_t buf_size = 0;
    if (iSizeInWords > 0) { buf_size = static_cast<size_t>(iSizeInWords * 4); }

    auto io_type = eroil::iosb::IoType::SLOT;
    if (iOffsetMode == static_cast<int>(eroil::iosb::IoType::OFFSET)) {
        io_type = eroil::iosb::IoType::OFFSET;
    }

    auto sem = static_cast<eroil::sem_handle>(iSem);
    auto siosb = static_cast<eroil::iosb::SendIosb*>(pIosb);

    uint32_t num_iosb = 0;
    if (iNumIosbs > 0) { num_iosb = static_cast<uint32_t>(iNumIosbs); }

    eroil::hndl::SendHandle* handle = eroil::open_send_label(
        label,
        buf,
        buf_size,
        io_type,
        sem,
        siosb,
        num_iosb
    );

    return static_cast<void*>(handle);
}

void NAE_Send_Label(void* iHandle,
                    char* pBuffer,
                    int iMsgSizeInWords,
                    int iSendOffsetInBytes,
                    int iReceiveOffsetInBytes) {

    // convert to expected types
    auto handle = static_cast<eroil::hndl::SendHandle*>(iHandle); 
    auto buf = reinterpret_cast<std::byte*>(pBuffer);

    size_t buf_size = 0;
    if (iMsgSizeInWords > 0) { buf_size = static_cast<size_t>(iMsgSizeInWords * 4); }

    size_t send_offset = 0;
    if (iSendOffsetInBytes > 0) { send_offset = static_cast<size_t>(iSendOffsetInBytes); }

    size_t recv_offset = 0;
    if (iReceiveOffsetInBytes > 0) { recv_offset = static_cast<size_t>(iReceiveOffsetInBytes); }

    eroil::send_label(
        handle,
        buf,
        buf_size,
        send_offset,
        recv_offset
    );
}

void NAE_Close_Send_Label(void* iHandle) { 
    eroil::close_send_label(static_cast<eroil::hndl::SendHandle*>(iHandle));
}

void* NAE_Open_Receive_Label(int iLabel,
                             int iForwardLabel,
                             char* pBuffer,
                             int iNumSlots,
                             int iSizeInWords,
                             char* pAuxBuffer,
                             void* iSem,
                             void* pIosb,
                             int iNumIosbs,
                             int iSignalMode) {

    // forward label is alawys -1, log if it isnt for the future
    if (iForwardLabel != -1) {
        ERR_PRINT("expected forward label to always be -1, but got forward label=", iForwardLabel);
    }

    // convert to expected types
    eroil::Label label = static_cast<eroil::Label>(iLabel);
    std::byte* buf = reinterpret_cast<std::byte*>(pBuffer);

    size_t buf_size = 0;
    if (iSizeInWords > 0) { buf_size = static_cast<size_t>(iSizeInWords * 4); }

    uint32_t num_slots = 0;
    if (iNumSlots > 0) { num_slots = static_cast<uint32_t>(iNumSlots); }

    std::byte* aux_buf = reinterpret_cast<std::byte*>(pAuxBuffer);
    auto sem = static_cast<eroil::sem_handle>(iSem);
    auto riosb = static_cast<eroil::iosb::ReceiveIosb*>(pIosb);

    uint32_t num_iosb = 0;
    if (iNumIosbs > 0) { num_iosb = static_cast<uint32_t>(iNumIosbs); }

    eroil::hndl::RecvHandle* handle = eroil::open_recv_label(
        label,
        buf,
        buf_size,
        num_slots,
        aux_buf,
        sem,
        riosb,
        num_iosb,
        eroil::iosb::to_signal_mode(static_cast<int32_t>(iSignalMode))
    );

    return static_cast<void*>(handle);
}

void NAE_Update_Receive_Label(
    void* /*iHandle*/,
    char* /*pBuffer*/,
    char* /*pAuxBuffer*/,
    int /*iNumSlots*/,
    int /*iSizeInWords*/
) {
    ERR_PRINT(" someone called this ?? weird, it shouldnt be used");
    // this is implemented by NAE but unused
    // updates buffers to point to the new addrs provided
    // maybe changes the size? that is going to be a pain
}

void NAE_Close_Receive_Label(void* iHandle) {
    eroil::close_recv_label(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

int NAE_Receive_Count(void* iHandle) {
    uint32_t count = eroil::recv_count(static_cast<eroil::hndl::RecvHandle*>(iHandle));
    return static_cast<int>(count);
}

void NAE_Receive_Dismiss(void* iHandle, int iCount) {
    auto handle = static_cast<eroil::hndl::RecvHandle*>(iHandle);
    uint32_t count = 0;
    if (iCount > 0) { count = static_cast<uint32_t>(iCount); }

    eroil::recv_dismiss(handle, count);
}

void NAE_Receive_Idle(void* iHandle) {
    eroil::recv_idle(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

void NAE_Receive_Resume(void* iHandle) {
    eroil::recv_resume(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

void NAE_Receive_Reset(void* iHandle) {
    eroil::recv_reset(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

void NAE_Receive_Redirect(void* iHandle) {
    eroil::recv_redirect(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

int NAE_Get_Message_Label(void* pIosb) {
    int32_t label = eroil::get_msg_label(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(label);
}

int NAE_Get_Message_Status(void* pIosb) {
    int32_t status = eroil::get_msg_status(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(status);
}

int NAE_Get_Message_Size(void* pIosb) {
    int32_t size = eroil::get_msg_size(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(size);
}

void* NAE_Get_Message_Address(void* pIosb) {
    void* addr = eroil::get_msg_address(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return addr;
}

int NAE_Get_Message_Offset(void* pIosb) {
    int32_t offset = eroil::get_msg_offset(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(offset);
}

void* NAE_Get_Message_Buffer(void* pIosb) {
    void* buf = eroil::get_msg_buffer(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return buf;
}

int NAE_Get_Message_Slot(void* pIosb) {
    int32_t slot = eroil::get_msg_slot(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(slot);
}

void NAE_Get_Message_Timestamp(void* pIosb, double* pTimeStamp) {
    eroil::get_msg_timestamp(
        static_cast<eroil::iosb::Iosb*>(pIosb),
        pTimeStamp
    );
}

void NAE_Current_Time(void* pTime) {
    eroil::get_current_time(
        static_cast<eroil::iosb::RTOSTime*>(pTime)
    );
}

void NAE_Write_Event_Log() {
    eroil::write_event_log();
}

void NAE_Write_Event_Log_Dir(const char* directory) {
    std::string dir = std::string(directory);
    eroil::write_event_log(dir);
}