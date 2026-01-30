#ifdef __cplusplus
    // if compiled as C++, prevent name mangling
    extern "C" {
#endif

#ifndef EROIL_C_H
#define EROIL_C_H

// C include file
// either import this into a C project or eroil_cpp.h for a C++ project

int NAE_Init(int iModuleId, int iProgramIDOffset, int iManager_CPU_ID, int iMaxNumCpus, int iNodeId);
int NAE_Is_Agent_Up(int iNodeId);
int NAE_Get_Node_ID(void);
int NAE_Get_ROIL_Node_ID(void);

void* NAE_Open_Send_Label(
    int iLabel,
    void* pSendBuffer,
    int iSizeInWords,
    int iOffsetMode,
    void* iSem,
    void* pIosb,
    int iNumIosbs
);

void NAE_Send_Label(
    void* iHandle,
    char* pBuffer,
    int iMsgSizeInWords,
    int iSendOffsetInBytes,
    int iReceiveOffsetInBytes
);

void NAE_Close_Send_Label(void* iHandle);

void* NAE_Open_Receive_Label( 
    int iLabel,
    int iForwardLabel,
    char* pBuffer,
    int iNumSlots,
    int iSizeInWords,
    char* pAuxBuffer,
    void* iSem,
    void* pIosb,
    int iNumIosbs,
    int iSignalMode
);

void NAE_Update_Receive_Label(
    void* iHandle,
    char* pBuffer,
    char* pAuxBuffer,
    int iNumSlots,
    int iSizeInWords
);

void NAE_Close_Receive_Label(void* iHandle);
int NAE_Receive_Count(void* iHandle);
void NAE_Receive_Dismiss(void* iHandle, int iCount);
void NAE_Receive_Idle(void* iHandle);
void NAE_Receive_Resume(void* iHandle);
void NAE_Receive_Reset(void* iHandle);
void NAE_Receive_Redirect(void* iHandle);
int NAE_Get_Message_Label(void* pIosb);
int NAE_Get_Message_Status(void* pIosb);
int NAE_Get_Message_Size(void* pIosb);
void* NAE_Get_Message_Address(void* pIosb);
int NAE_Get_Message_Offset(void* pIosb);
void* NAE_Get_Message_Buffer(void* pIosb);
int NAE_Get_Message_Slot(void* pIosb);
void NAE_Get_Message_Timestamp(void* pIosb, double* pTimeStamp);
void NAE_Current_Time(void* pTime);
#endif

#ifdef __cplusplus
}
#endif