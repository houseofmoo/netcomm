#include <eROIL/eroil_c.h>
#include <eROIL/print.h>
#include "types/handles.h"
#include "types/types.h"
#include "eroil.h"

// implementation for eroil_c.h
// calls into eroil.cpp

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

    eroil::iosb::IoType io_type = eroil::iosb::IoType::SLOT;
    if (iOffsetMode == static_cast<int>(eroil::iosb::IoType::OFFSET)) {
        io_type = eroil::iosb::IoType::OFFSET;
    }

    auto handle = eroil::open_send_label(
        static_cast<eroil::Label>(iLabel),
        static_cast<uint8_t*>(pSendBuffer),
        static_cast<int32_t>(iSizeInWords * 4),
        io_type,
        static_cast<eroil::sem_handle>(iSem),
        static_cast<eroil::iosb::SendIosb*>(pIosb),
        static_cast<int32_t>(iNumIosbs)
    );

    return static_cast<void*>(handle);
}

void NAE_Send_Label(void* iHandle,
                    char* pBuffer,
                    int iMsgSizeInWords,
                    int iSendOffsetInBytes,
                    int iReceiveOffsetInBytes) {
    eroil::send_label(
        static_cast<eroil::hndl::SendHandle*>(iHandle),
        reinterpret_cast<uint8_t*>(pBuffer),
        static_cast<int32_t>(iMsgSizeInWords * 4),
        static_cast<int32_t>(iSendOffsetInBytes),
        static_cast<int32_t>(iReceiveOffsetInBytes)
    );
}

void NAE_Close_Send_Label(void* iHandle) { 
    eroil::close_send(static_cast<eroil::hndl::SendHandle*>(iHandle));
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
        ERR_PRINT(__func__, " expected forward label to always be -1, but got forward label=", iForwardLabel);
    }

    eroil::iosb::SignalMode smode = eroil::iosb::SignalMode::OVERWRITE;
    if (iSignalMode == 1) {
        smode = eroil::iosb::SignalMode::BUFFER_FULL;
    } else if (iSignalMode == 2) {
        smode = eroil::iosb::SignalMode::EVERY_MESSAGE;
    }

    auto handle = eroil::open_recv_label(
        static_cast<int32_t>(iLabel),
        reinterpret_cast<uint8_t*>(pBuffer),
        static_cast<int32_t>(iSizeInWords * 4),
        static_cast<int32_t>(iNumSlots),
        reinterpret_cast<uint8_t*>(pAuxBuffer),
        static_cast<eroil::sem_handle>(iSem),
        static_cast<eroil::iosb::ReceiveIosb*>(pIosb),
        static_cast<int32_t>(iNumIosbs),
        smode
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
    ERR_PRINT(__func__, " someone called this ?? weird, it shouldnt be used");
    // this is implemented by NAE but unused
    // updates buffers to point to the new addrs provided
    // maybe changes the size? that is going to be a pain
}

void NAE_Close_Receive_Label(void* iHandle) {
    eroil::close_recv_label(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

int NAE_Receive_Count(void* iHandle) {
    auto count = eroil::recv_count(static_cast<eroil::hndl::RecvHandle*>(iHandle));
    return static_cast<int>(count);
}

void NAE_Receive_Dismiss(void* iHandle, int iCount) {
    eroil::recv_dismiss(
        static_cast<eroil::hndl::RecvHandle*>(iHandle),
        static_cast<int32_t>(iCount)
    );
}

void NAE_Receive_Idle(void* /*iHandle*/) {
    // not implemented
}

void NAE_Receive_Resume(void* /*iHandle*/) {
    // not implemented
}

void NAE_Receive_Reset(void* iHandle) {
    eroil::recv_reset(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

void NAE_Receive_Redirect(void* iHandle) {
    eroil::recv_redirect(static_cast<eroil::hndl::RecvHandle*>(iHandle));
}

int NAE_Get_Message_Label(void* pIosb) {
    auto label = eroil::get_msg_label(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(label);
}

int NAE_Get_Message_Status(void* pIosb) {
    auto status = eroil::get_msg_status(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(status);
}

int NAE_Get_Message_Size(void* pIosb) {
    auto size = eroil::get_msg_size(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(size);
}

void* NAE_Get_Message_Address(void* pIosb) {
    auto addr = eroil::get_msg_address(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return addr;
}

int NAE_Get_Message_Offset(void* pIosb) {
    auto offset = eroil::get_msg_offset(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return static_cast<int>(offset);
}

void* NAE_Get_Message_Buffer(void* pIosb) {
    auto buf = eroil::get_msg_buffer(
        static_cast<eroil::iosb::Iosb*>(pIosb)
    );
    return buf;
}

int NAE_Get_Message_Slot(void* pIosb) {
    auto slot = eroil::get_msg_slot(
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