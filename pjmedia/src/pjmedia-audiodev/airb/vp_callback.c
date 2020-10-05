#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <Typedef.h>
#include <JitterBf.h>
#include <RTPDec.h>
#include <Statics.h>
#if ENABLE_T38_FAX_RELAY
#include <T38Dec.h>
#endif
#include <EvntHndl.h>

#include "vp_interface.h"

/******************************************************/
/* VoicePacketizer Simple Demo (C) 2000 by AudioCodes */
/******************************************************/

/*--------------------------------------------------------------------
 * ROUTINE:   SendToNetwork
 *
 * DESCRIPTION:
 *   Sends a RTP packet to NetWork.
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to send
 * [i]char *FramePtr   - pointer to the beginning of RTP packet to be transmitted (to the beginning of RTP header).      
 * [i]char FrameLen    - length of RTP payload (including header) to be transmitted.
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
void SendToNetwork(int CID, char *FramePtr, short FrameLen)
{
    //VP_FUNC_ENT();

    /* Simulate a network loopback by calling the decoder directly
	// Use the channel TxCID parameter for channel cross connections.*/
    /* RTPDecoder(ChInfo[CID].TxCID, FramePtr, (short)FrameLen); */

    airb_ca_cb(FramePtr, (short)FrameLen);

    //VP_FUNC_LEV();
}

/*--------------------------------------------------------------------
 * ROUTINE:   SendRTCPToNetwork
 *
 * DESCRIPTION:
 *   Sends a RTCP frame to NetWork. 
 *   In AVP case Network is short circuit and there for the frame is directly sent
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to send
 * [i]char *FramePtr   - pointer to the beginning of RTCP packet to be transmitted (to the beginning of RTP header).      
 * [i]char FrameLen    - length of RTCP payload (including header) to be transmitted.
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
void SendRTCPToNetwork(int CID, char *FramePtr, short FrameLen)
{
    //VP_FUNC_ENT();

    /* Simulate a network loopback by calling the decoder directly
	// Use the channel TxCID parameter for channel cross connections.*/
    /* RTPDecoder(ChInfo[CID].TxCID, FramePtr, (short)FrameLen); */

    //VP_FUNC_LEV();
}

#if ENABLE_T38_FAX_RELAY
/*--------------------------------------------------------------------
 * ROUTINE:   SendT38ToNetwork
 *
 * DESCRIPTION:
 *   Sends a T38 packet to NetWork.
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to send
 * [i]char *FramePtr   - pointer to the beginning of T38 packet to be transmitted (to the beginning of UDP Payload).      
 * [i]char FrameLen    - length of T.38 payload (without IP/UDP header) to be transmitted.
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
void SendT38ToNetwork(int CID, char *FramePtr, short FrameLen)
{
    VP_FUNC_ENT();

    /* Simulate a network loopback by calling the T38 decoder directly
	// Use the channel TxCID parameter for channel cross connections.*/
    T38Decoder(ChInfo[CID].TxCID, FramePtr, (short)FrameLen, 1);

    VP_FUNC_LEV();
}
#endif

/*--------------------------------------------------------------------
 * ROUTINE:   GetNewFramePtr
 *
 * DESCRIPTION:
 *   This routine Allocates memory buffer to place the new RTP packet. 
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to request
 * [o]char **pPayload  - pointer to start of RTP payload (=pointer to start of RTP header+12)
 *
 * RESULTS:
 *   char *            - pointer to allocated memory buffer for RTP packet 
 *                       (points to the beginning of RTP header)
 *--------------------------------------------------------------------*/
char *GetNewFramePtr(int CID, char **pPayload)
{
    /* A static buffer for holding the RTP packets to be transmitted
	// For true VoIP applications, the function should return a pointer
	// to a LAN buffer, and not a static storage.*/
    static char NITxBuff[MAX_VOIP_SIZE];

    //VP_FUNC_ENT();

    /* The payload will start after the RTP header*/
    *pPayload = NITxBuff + sizeof(struct TRTPHeader);

    //VP_FUNC_LEV();

    /* Return a pointer to the buffer*/
    return (NITxBuff);
}

/*--------------------------------------------------------------------
 * ROUTINE:   GetNewRTCPFramePtr
 *
 * DESCRIPTION:
 *   This routine Allocates memory buffer to place the new RTCP frame 
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to request
 *
 * RESULTS:
 *   char *            - pointer to allocated memory buffer for RTCP packet 
 *                       (points to the beginning of RTP header)
 *--------------------------------------------------------------------*/
char *GetNewRTCPFramePtr(int CID)
{
    /* A static buffer for holding the RTP packets to be transmitted
	// For true VoIP applications, the function should return a pointer
	// to a LAN buffer, and not a static storage.*/
    static char NITxBuff[MAX_VOIP_SIZE];

    //VP_FUNC_ENT();
    //VP_FUNC_LEV();

    /* Return a pointer to the buffer*/
    return (NITxBuff);
}

/*--------------------------------------------------------------------
 * ROUTINE:   GetNewT38FramePtr
 *
 * DESCRIPTION:
 *   This routine Allocates memory buffer to place the new T.38 packet. 
 *	 The Difference from the other GetNewXXXFramePtr is the header location
 *	 pPayload is at the start of the packet for NI and TPCIPacketHeader away
 *	 for PCI
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to request
 * [o]char **pPayload  - pointer to start of T.38 payload (=pointer to start of UDP Payload)
 *
 * RESULTS:
 *   char *            - pointer to allocated memory buffer for T.38 packet 
 *                       (points to the beginning of header)
 *--------------------------------------------------------------------*/
char *GetNewT38FramePtr(int CID, char **pPayload)
{
    /* A static buffer for holding the RTP packets to be transmitted
	// For true VoIP applications, the function should return a pointer
	// to a LAN buffer, and not a static storage.*/
    static char NITxBuff[MAX_VOIP_SIZE];

    //VP_FUNC_ENT();

    /* In this case (T38) the payload is at the buffer's beginning.*/
    *pPayload = NITxBuff;

    //VP_FUNC_LEV();

    /* Return a pointer to the buffer*/
    return (NITxBuff);
}

/*--------------------------------------------------------------------
 * ROUTINE:   ErrorHandler
 *
 * DESCRIPTION:
 *   User supplied function for handling errors.
 *
 * ARGUMENTS:
 * [i]TErrorMsgType ErrorMsgType   - Error type (fatal, recoverable, info etc.)
 * [i]int ErrorCode                - Error Code (according to typedef.h list)
 * [i]char *File                   - originating file name
 * [i]int LineInFile               - originating file line number
 * [i]ErrorMsgSourceModule         - originating module 
 * [i]int AC4804Index              - device in which error was detected
 * [i]int CID                      - Channel in which error was detected
 * [i]char *Format                 - printf format string
 * [i]...                          - arguments for printf
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
void ErrorHandler(enum TErrorMsgType ErrorMsgType, int ErrorCode, char *File, int LineInFile, enum acTErrorMsgSourceModule ErrorMsgSourceModule, int AC4804Index, int CID, char *Format, ...)
{
    static char ErrStr[256];
    va_list pArguments;

    VP_FUNC_ENT();

    /* generate the error string using the printf format and the variable number arguments*/
    va_start(pArguments, Format);
    vsprintf(ErrStr, Format, pArguments);

    switch (ErrorMsgType)
    {
    case InfoMsg:
        printf("Message: %s\n", ErrStr);
        break;

    case RecoverableErrorMsg:
        printf("Error: %s\n", ErrStr);
        break;

    case FatalErrorMsg:
        printf("FatalError: %s\n", ErrStr);
        /* Infinite loop :*/
        while (1)
            ;
        break;
    default:
        break;
    }

    VP_FUNC_LEV();
}

/*--------------------------------------------------------------------
 * ROUTINE:   SignalEvent
 *
 * DESCRIPTION:
 *   Notifies user's application that an event has occurred (such as DTMF Digit Received etc)
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel where event has occurred
 * [i]int  EventType   - Type of event (see Typedef.h for event types definitions)         
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
void SignalEvent(int CID, int EventType)
{
    /* The current event information*/
    acTEventInfo EventInfo;
    struct acTDigit Digit;

    VP_FUNC_ENT();

#if RTCP_XR_SUPPORTED
    acVqmonAlertInfo RTCPXRAlert;
    vqmon_rtcpxrreport_t RTCPXRInfo;
#endif /*RTCP_XR_SUPPORTED*/

    static char EventText[500];
    int DisplayeventInfo;

    /* Initialize the event string */
    strcpy(EventText, "<no event>");

    /* By default, display the event */
    DisplayeventInfo = 1;

    /* Get the event and process it*/
    switch (GetEvent((short)CID, &EventInfo))
    {
    case EV_DIGIT:
        /* Copy the single digit from the event info */
        Digit = EventInfo.u.DigitInfo;
        if (Digit.Digit != -1)
            sprintf(EventText, "Digit Detected: Digit:%d  OnTime=%d  OffTime=%d  SigSys=%d", Digit.Digit, Digit.DigitOnTime, Digit.InterDigitTime, Digit.SignalingSystem);
        break;

    case EV_END_DIAL:
        sprintf(EventText, "Dial Ended");
        break;

    case EV_DETECT_FAX:
        sprintf(EventText, "Fax Detected from %s\n",
                EventInfo.u.FaxInfo.Network_Tdm ? "Network" : "TDM");
        break;

    case EV_DETECT_FAX_PAGE:
        sprintf(EventText, "New Fax Page\n");
        break;

    case EV_END_FAX:
        sprintf(EventText, "Fax Ended %d pages were sent\n", TxChInfo[CID].NumberOfFaxPages);
        break;

    case EV_DETECT_MODEM:
        sprintf(EventText, "Modem Detected\n");
        break;

    case EV_END_MODEM:
        sprintf(EventText, "Modem Ended\n");
        break;

    case EV_DETECT_ANSWER_TONE:
        sprintf(EventText, "Answer Tone Detected\n");
        break;

    case EV_DETECT_MODEM_RETRAIN:
        sprintf(EventText, "Modem Retrain Detected");
        break;

    case EV_END_MODEM_RETRAIN:
        sprintf(EventText, "Modem Retrain Ended");
        break;

    case EV_2250HZ_DETECT_MODEM:
        sprintf(EventText, "Modem Bypass Mode - 2250Hz Detected");
        break;

    case EV_DETECT_CALLER_ID:
        sprintf(EventText, "Caller ID: %c%c/%c%c %c%c:%c%c #%s : %s %s",
                EventInfo.u.CallerID.Date[0], EventInfo.u.CallerID.Date[1],
                EventInfo.u.CallerID.Date[2], EventInfo.u.CallerID.Date[3],
                EventInfo.u.CallerID.Time[0], EventInfo.u.CallerID.Time[1],
                EventInfo.u.CallerID.Time[2], EventInfo.u.CallerID.Time[3],
                EventInfo.u.CallerID.Number,
                EventInfo.u.CallerID.Name,
                EventInfo.u.CallerID.Status ? "(Error)" : "");

        break;

    case EV_RTCP:
        sprintf(EventText, "RTCP event");
        DisplayeventInfo = 0;
        break;

    case EV_RTCP_EXT_RECEIVED:
        sprintf(EventText, "RTCP event");
        DisplayeventInfo = 0;
        break;

#if RTCP_XR_SUPPORTED

    case EV_RTCPXR_ALERT:
        memcpy(&RTCPXRAlert, &EventInfo.u.RTCPXRAlert, sizeof(struct acVqmonAlertInfo));
        sprintf(EventText, "RTCP-XR Alert State: %d  Alert Type: %d\n", RTCPXRAlert.alertState, RTCPXRAlert.alertType);
        break;

    case EV_RTCPXR_INFO:
        memcpy(&RTCPXRInfo, &EventInfo.u.RTCPXRInfo, sizeof(struct _vqmon_rtcpxrreport_s));
        sprintf(EventText, " EV_RTCPXR_INFO   nLossRate=%d nDiscardRate=%d nAvgBurstDensity=%d \
								nAvgGapDensity=%d nAvgBurstDuration=%d nAvgGapDuration=%d \
								nRTDelayMs=%d nEndSystemDelayMs=%d nSignalLevel_dBm=%d \
								nNoiseLevel_dBm=%d nRERL_dB=%d nMinGapSize=%d nRFactor=%d \
								nExtRFactor = %d nMOS_LQ=%d nMOS_CQ=%d fRXConfig=%d \
								nJBNomDelayMs=%d nJBMaxDelayMs=%d nJBAbsMaxDelayMs=%d\n",
                RTCPXRInfo.nLossRate, RTCPXRInfo.nDiscardRate, RTCPXRInfo.nAvgBurstDensity, RTCPXRInfo.nAvgGapDensity,
                RTCPXRInfo.nAvgBurstDuration, RTCPXRInfo.nAvgGapDuration, RTCPXRInfo.nRTDelayMs, RTCPXRInfo.nEndSystemDelayMs,
                RTCPXRInfo.nSignalLevel_dBm, RTCPXRInfo.nNoiseLevel_dBm, RTCPXRInfo.nRERL_dB, RTCPXRInfo.nMinGapSize,
                RTCPXRInfo.nRFactor, RTCPXRInfo.nExtRFactor, RTCPXRInfo.nMOS_LQ, RTCPXRInfo.nMOS_CQ, RTCPXRInfo.fRXConfig,
                RTCPXRInfo.nJBNomDelayMs, RTCPXRInfo.nJBMaxDelayMs, RTCPXRInfo.nJBAbsMaxDelayMs);

        break;

    case EV_RTCPXR_REMOTE_INFO:
        memcpy(&RTCPXRInfo, &EventInfo.u.RTCPXRInfo, sizeof(struct _vqmon_rtcpxrreport_s));

        sprintf(EventText, " EV_RTCPXR_REMOTE_INFO   nLossRate=%d nDiscardRate=%d nAvgBurstDensity=%d \
								nAvgGapDensity=%d nAvgBurstDuration=%d nAvgGapDuration=%d \
								nRTDelayMs=%d nEndSystemDelayMs=%d nSignalLevel_dBm=%d \
								nNoiseLevel_dBm=%d nRERL_dB=%d nMinGapSize=%d nRFactor=%d \
								nExtRFactor = %d nMOS_LQ=%d nMOS_CQ=%d fRXConfig=%d \
								nJBNomDelayMs=%d nJBMaxDelayMs=%d nJBAbsMaxDelayMs=%d\n",
                RTCPXRInfo.nLossRate, RTCPXRInfo.nDiscardRate, RTCPXRInfo.nAvgBurstDensity, RTCPXRInfo.nAvgGapDensity,
                RTCPXRInfo.nAvgBurstDuration, RTCPXRInfo.nAvgGapDuration, RTCPXRInfo.nRTDelayMs, RTCPXRInfo.nEndSystemDelayMs,
                RTCPXRInfo.nSignalLevel_dBm, RTCPXRInfo.nNoiseLevel_dBm, RTCPXRInfo.nRERL_dB, RTCPXRInfo.nMinGapSize,
                RTCPXRInfo.nRFactor, RTCPXRInfo.nExtRFactor, RTCPXRInfo.nMOS_LQ, RTCPXRInfo.nMOS_CQ, RTCPXRInfo.fRXConfig,
                RTCPXRInfo.nJBNomDelayMs, RTCPXRInfo.nJBMaxDelayMs, RTCPXRInfo.nJBAbsMaxDelayMs);
        break;

#endif /*RTCP_XR_SUPPORTED*/

    case EV_ON_HOOK:
        /* Relay the signaling change to the channel connected to the originating */
        SetEM(ChInfo[CID].TxCID, ON_HOOK, 0);

        sprintf(EventText, "On-Hook Detected (Ring #%d)", TxChInfo[CID].u.vp.EMRingsCount);
        break;

    case EV_OFF_HOOK:
        /* Relay the signaling change to the channel connected to the originating */
        SetEM(ChInfo[CID].TxCID, OFF_HOOK, 0);

        sprintf(EventText, "Off-Hook/Ring Detected");
        break;

    case EV_FLASH_HOOK:
        SetEM(ChInfo[CID].TxCID, OFF_HOOK, 0);
        sprintf(EventText, "Flash-Hook Detected");
        break;

    case EV_TELEPHONY_SIGNAL:
        sprintf(EventText, "Telephony Signal: Telephony Signal Detected   Signal Code:%d  Duration:%d msec  IBSDirection = %d SignalDetection = %d", EventInfo.u.TelephonySignalInfo.SignalCode, EventInfo.u.TelephonySignalInfo.Duration, EventInfo.u.TelephonySignalInfo.DetectionDirection, EventInfo.u.TelephonySignalInfo.TelephonySignalDetection);
        break;

    case EV_TELEPHONY_SIGNAL_END:
        sprintf(EventText, "Telephony Signal: Telephony Signal Ended   Signal Code:%d  IBSDirection = %d ", EventInfo.u.TelephonySignalInfo.SignalCode, EventInfo.u.TelephonySignalInfo.DetectionDirection);
        break;

    case EV_CAS_CHANGED:
        sprintf(EventText, "CAS changed");
        break;

    case EV_DETECT_CALL_PROGRESS_TONE:
        sprintf(EventText, "Call Progress detected: Index=%d InterEventTime=%d", EventInfo.u.CallProgressInfo.ToneIndex, EventInfo.u.CallProgressInfo.InterEventTime);
        break;

    case EV_END_CALL_PROGRESS_TONE:
        sprintf(EventText, "Call Progress tone ended InterEventTime=%d", EventInfo.u.CallProgressInfo.InterEventTime);
        break;

    case EV_END_SENDING_CAS:
        sprintf(EventText, "End Sending CAS ");
        break;

    case EV_END_PLAYING_CALL_PROGRESS_TONE:
        sprintf(EventText, "End Playing call progress tone ");
        break;

    case EV_END_VOICE_PROMPT:
        sprintf(EventText, "End Playing voice prompt ");
        break;

    default:
        sprintf(EventText, "Unknown.");
        break;
    }

    /* Display the event information */
    if (DisplayeventInfo)
        printf("Ch %d Event: %s\n", CID, EventText);

    VP_FUNC_LEV();
}

/*--------------------------------------------------------------------
 * ROUTINE:   FirstIncomingPacketDetected
 *
 * DESCRIPTION:
 *   Signals application that the first packet of a certain type was 
 *    detected coming from network.
 *   This function will be used to handle NAT (Network Address Translation) devices 
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel to send
 * [i]int PacketType   - 0 for RTP, 1 for RTCP, 2 for T38.      
 * [i]char *pBuffer    - Pointer to beginning of RTP header.
 *
 * RESULTS:
 *   none
 *--------------------------------------------------------------------*/
int FirstIncomingPacketDetected(int PacketType, int CID, char *pBuffer)
{
    VP_FUNC_ENT();
    VP_FUNC_LEV();

    return (0);
}

/*--------------------------------------------------------------------
 * ROUTINE:   DelayForDeviceResponse()
 *
 * DESCRIPTION:
 *   Performs a constnat delay. Used when waiting for the DSP to respond.
 *
 * ARGUMENTS:
 *   none
 *
 * RESULTS:
 *   none
 * 
 * ALGORITHM :
 * - Wait (Sleep) for DELAY_FOR_DEVICE_TIME ms.
 *
 * NOTES :
 * 
 * MODIFICATIONS :
 * 
 *--------------------------------------------------------------------*/
void DelayForDeviceResponse(void)
{
    VP_FUNC_ENT();

    /* In this demo a 10ms delay is used. The required delay is much smaller. */
    usleep(10000);

    VP_FUNC_LEV();
}

/* VOICE PACKETIZER DEBUG RECORDING CALLBACK FUNCTIONS */
/*--------------------------------------------------------------------
 * ROUTINE:   VP_GetSystemTime
 *
 * DESCRIPTION:
 *     Getting time stamp from the system clock for inner VoicePacketizer purposes.
 *
 * ARGUMENTS:
 * [o] None.
 *
 * RESULTS:
 *     Time stamp that was measured by the system in milli seconds unit.
*--------------------------------------------------------------------*/
unsigned int VP_GetSystemTime(void)
{
    //VP_FUNC_ENT();
    //VP_FUNC_LEV();

    return 0;
}

/*--------------------------------------------------------------------
* ROUTINE:   SendRecPacketToIO
*
* DESCRIPTION:
*      Sending the record packets to the IO device. Time consuming of 
*  this function should'nt be longer than similar functions such as
*  SendToNetwork.
*
*
* ARGUMENTS:
* [i] short *RecPacket      - Record packet to send to IO.
* [i] short RecPacketLength - Record Packet Length.
*
* RESULTS:
*   -1 - Failure
*    0 - Success
*--------------------------------------------------------------------*/
int SendRecPacketToIO(short *RecPacket, short RecPacketLength)
{
    //VP_FUNC_ENT();
    //VP_FUNC_LEV();

    return 0;
}

/*--------------------------------------------------------------------
* ROUTINE:   GetNewRecordFramePtr
*
* DESCRIPTION:
*      Getting a new alocated frame for the record packet to be sent.
*
* ARGUMENTS:
* [i] int PacketLength - The length of the buffer needed.
* [o] char **RecPacket - Pointer to the alocated frame.
*
* RESULTS:
*   -1 - Failure
*    0 - Success
*--------------------------------------------------------------------*/
int GetNewRecordFramePtr(int PacketLength, char **RecPacket)
{
    //VP_FUNC_ENT();
    //VP_FUNC_LEV();

    return 0;
}

/*--------------------------------------------------------------------
 * ROUTINE:   SendTDMNetworkPacketToNetwork
 *
 * DESCRIPTION:
 *  Sending the TDM and Network recording packets to network. The recording
 *  packets will be G.711 format regardless the channel status and configuration.
 *  Time consuming of this function should'nt be longer than similar functions such as
 *  SendToNetwork.
 *
 *
 * ARGUMENTS:
 * [i]int  CID         - Channel from which the recording packet refers to.
 * [i]char *FramePtr   - pointer to the beginning of recording packet to be transmitted (to the beginning of RTP header).      
 * [i]char FrameLen    - length of RTP payload (including header) to be transmitted.
 * [i]acTNetworkTDMRecordedPacket NetworkTDMRecordedPacket - Indicated whether it's a network or TDM recorded packet 
 *
 * RESULTS:
 *   -1 - Failure
 *    0 - Success
 *--------------------------------------------------------------------*/
int SendTDMNetworkPacketToNetwork(int CID, char *FramePtr, short FrameLen, acTNetworkTDMRecordedPacket NetworkTDMRecordedPacket)
{
    //VP_FUNC_ENT();
    //VP_FUNC_LEV();

    return 0;
}

/*--------------------------------------------------------------------
* ROUTINE:   GetDeviceVersionByTemplate
*
* DESCRIPTION:
*   Returns the required DSP device version number according to a
*   selected template.
*
* ARGUMENTS:
* [i]int TemplateIndex              - index of the selected template
* [i]int PointerToRTPPayload_Header - device index
*
* RESULTS:
* [o]int                            - version number of this device
*--------------------------------------------------------------------*/
int GetDeviceVersionByTemplate(int TemplateIndex, int DeviceIndex)
{
    VP_FUNC_ENT();

    /* For this board, only the default template is implemented - */
    /* The first DSP version (#0) is loaded to all devices */
    switch (TemplateIndex)
    {
#if (AC48X_DEVICE_TYPE == AC486XX_DEVICE)

#ifdef AC48624AE3
    case AC48624AE3:
        /* AC48624AE3 version for all DSPs */
        return AC48624AE3;
#endif
#ifdef AC48616AE3
    case AC48616AE3:
        /* AC48616AE3 version for all DSPs */
        return AC48616AE3;
#endif

#ifdef AC48620AE9
    case AC48620AE9:
        /* AC48620AE9 version for all DSPs */
        return AC48620AE9;
#endif

#ifdef AC48612AE7
    case AC48612AE7:
        /* AC48612AE7 version for all DSPs */
        return AC48612AE7;
#endif
#ifdef AC48612AE5
    case AC48612AE5:
        /* AC48612AE5 version for all DSPs */
        return AC48612AE5;
#endif
#ifdef AC48616AE8
    case AC48616AE8:
        /* AC48616AE8 version for all DSPs */
        return AC48616AE8;
#endif

#ifdef AC47616AE3
    case AC47616AE3:
        /* AC47616AE3 version for all DSPs */
        return AC47616AE3;
#endif
#elif (AC48X_DEVICE_TYPE == AC4864X_DEVICE)

#ifdef AC48648AB
    case AC48648AB:
        /* AC48648AB version for all DSPs */
        return AC48648AB;
#endif

#elif (AC48X_DEVICE_TYPE == AC4810XIM_DEVICE)

#ifdef AC48104IM4
    case AC48104IM4:
        /* AC48104IM4 version for all DSPs */
        return AC48104IM4;
#endif

#ifdef AC48204IM3
    case AC48204IM3:
        /* AC48204IM3 version for all DSPs */
        return AC48204IM3;
#endif

#ifdef AC48104IMB
    case AC48104IMB:
        /* AC48104IMB version for all DSPs */
        return AC48104IMB;
#endif

#ifdef AC48105IM4
    case AC48105IM4:
        /* AC48105IM4 version for all DSPs */
        return AC48105IM4;
#endif

#ifdef AC48105IMB
    case AC48105IMB:
        /* AC48105IMB version for all DSPs */
        return AC48105IMB;
#endif

#elif (AC48X_DEVICE_TYPE == AC481XX_DEVICE)
#ifdef AC48112AB
    case AC48112AB:
        /* AC48112AB version for all DSPs */
        return AC48112AB;

#endif
#elif (AC48X_DEVICE_TYPE == AC4880X_DEVICE)

#ifdef AC48801CE3
    case AC48801CE3:
        /* AC48801CE3 version for all DSPs */
        return AC48801CE3;
#endif

#ifdef AC48802CE3
    case AC48802CE3:
        /* AC48802CE3 version for all DSPs */
        return AC48802CE3;
#endif

#ifdef AC48802CE1
    case AC48802CE1:
        /* AC48802CE1 version for all DSPs */
        return AC48802CE1;
#endif

#ifdef AC48804CB
    case AC48804CB:
        /* AC48804CB version for all DSPs */
        return AC48804CB;
#endif

#elif (AC48X_DEVICE_TYPE == AC4830X_DEVICE)

#ifdef AC48302CE6
    case AC48302CE6:
        /* AC48302CE6 version for all DSPs */
        return AC48302CE6;
#endif

#ifdef AC48304CB
    case AC48304CB:
        /* AC48304CB version for all DSPs */
        return AC48304CB;
#endif
#ifdef AC48304CE1
    case AC48304CE1:
        /* AC48304CE1 version for all DSPs */
        return AC48304CE1;
#endif
#ifdef AC48304CE2
    case AC48304CE2:
        /* AC48304CE2 version for all DSPs */
        return AC48304CE2;
#endif

#endif
    default:
        return (-1);
    }
    /*return AC48648AB;*/

    VP_FUNC_LEV();
}
