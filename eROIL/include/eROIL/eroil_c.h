#ifdef __cplusplus
#    pragma once
extern "C" {
#endif

#ifndef EROIL_C_H
    #define EROIL_C_H
    #ifdef EROIL_LINUX
    #include <semaphore.h>
    #include <stddef.h>
    #include <stdint.h>
        #defime SEM_HANDLE sem_t*;
    #elif EROIL_WIN32
        #define SEM_HANDLE void*
    #else
        #error "Must define EROIL_LINUX or EROIL_WIN32"
    #endif

// THIS IS SO WE CAN REPLACE NAE with eROIL without having to change roil at all

int NAE_Init(int iModuleId, int iProgramIDOffset, int iManager_CPU_ID, int iMaxNumCpus, int iNodeId);
int NAE_Is_Agent_Up(int iNodeId);
int NAE_Get_Node_ID(void);
int NAE_Get_ROIL_Node_ID(void);

void* NAE_Open_Send_Label(
    int iLabel,
    void* pSendBuffer,
    int iSizeInWords,
    int iOffsetMode,
    SEM_HANDLE iSem,
    void* pIosb,
    int iNumIosbs
);

void NAE_Send_Label(
    void* iHandle,
    char* pBuffer,
    int iMsgSizeInWords,
    int iSendOffsetInBytes,
    int iReceiveOffsetInBytes  // the fuck is this for?
);

// implemented
void NAE_Close_Send_Label(void* iHandle);  // handle return from OpenSend(), change to void* at least?

// implemented
void* NAE_Open_Receive_Label(  // change return to void*? or type...
    int iLabel,
    int iForwardLabel,
    char* pBuffer,
    int iNumSlots,
    int iSizeInWords,
    char* pAuxBuffer,
    SEM_HANDLE iSem,
    void* pIosb,
    int iNumIosbs,
    int iSignalMode
);

// implemented
void NAE_Update_Receive_Label(
    void* iHandle,
    char* pBuffer,
    char* pAuxBuffer,
    int iNumSlots,
    int iSizeInWords
);

// implemented
void NAE_Close_Receive_Label(void* iHandle);

// implemented
int NAE_Receive_Count(void* iHandle);

// implemented
void NAE_Receive_Dismiss(void* iHandle, int iCount);

// not implemented
void NAE_Receive_Idle(void* iHandle);

// not implemented
void NAE_Receive_Resume(void* iHandle);

// implemented
void NAE_Receive_Reset(void* iHandle);

// implemented
void NAE_Receive_Redirect(void* iHandle);

// return pIosb->FC_Header.Destination_ID
int NAE_Get_Message_Label(void* pIosb);

// return pIosb->Status
int NAE_Get_Message_Status(void* pIosb);

// checks pIosb->Reserve2 == NAE_EVENT_RECEIVE
// TRUE:
//      return pIosb->MsgSize
// FALSE:
//      return pIosb->MsgSize/4
int NAE_Get_Message_Size(void* pIosb);

// return pIosb->MsgAddr
void* NAE_Get_Message_Address(void* pIosb);

// checks pIosb->Reserve2 == NAE_EVENT_RECEIVE
// TRUE:
//      return pIosb->FC_Header.Parameter
// FALSE:
//      return 0
int NAE_Get_Message_Offset(void* pIosb);

// checks pIosb->Reserve2 == NAE_EVENT_RECEIVE
// TRUE:
//      iLabel = pIosb->Reserve5
//      return ptr to globalReceiveTablePtr[iLabel]->Entry[0].sLocalRecceiverInfo.pEffectiveBuffer
// FALSE:
//      return ptr to pIosb->MsgAddr
void* NAE_Get_Message_Buffer(void* pIosb);

// checks pIosb->Reserve2 == NAE_EVENT_RECEIVE
// returns pIosb->Message_Slot, otherwise return 0
int NAE_Get_Message_Slot(void* pIosb);

// gets timestamp in pIosb and calls RTOS to convert that time
// to raw time and places anser in pTimeStamp
void NAE_Get_Message_Timestamp(void* pIosb, double* pTimeStamp);

// calls into RTOS to get the time
void NAE_Current_Time(void* pTime);
#endif

#ifdef __cplusplus
}
#endif