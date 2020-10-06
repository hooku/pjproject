/* 
 * interface to adapt between airb_dev.c and VP demo
 * most of the contents of this file are copy from
 * VoicePacketizeSimpleDemo.c
 */

#include <memory.h>
#include <stdio.h>
#include <time.h>

// VoicePacketizer Includes
#include <AC48xIF.h>
#include <EvntHndl.h>
#include <Init.h>
#include <JitterBf.h>
#include <RTCP.h>
#include <RTPDec.h>
#include <RTPEnc.h>
#include <Statics.h>
#include <Typedef.h>
#include <UserFunc.h>
#include <VoicePck.h>
#include <vpInit.h>
#include <vpStatics.h>
#if ENABLE_T38_FAX_RELAY
#include "T38Dec.h"
#endif

#include <pjmedia/rtp.h>

#include "../airb_dev.h"
#include "vp_interface.h"

enum TPCMLawSelect UsedPcmLaw = MuLaw;
int IdlePcm = 0xFF;

extern int InitializeEvaluationBoard();
static void vp_ConfigureUserPCMSetupInfo(struct TUserPCMSetupInfo *UserPCMInfo);
static int vp_ConfigureCallProgressTone(struct acTCallProgressTone *UserCallProgressTones);
static void vp_ConfigureUserChannelSetupInfo(TUserChannelSetupInfo *UserChannelSetupInfo, int ChannelID);
static void vp_ConnectChannels(TUserChannelSetupInfo *UserChannelInfo1, int CID1, TUserChannelSetupInfo *UserChannelInfo2, int CID2);
static void vp_OpenDefaultChannel(TUserChannelSetupInfo *UserChannelInfo1, int CID1);

void vp_Init_DSP()
{
    // VoicePacketizer configuration structure :
    // These structure are used as input arguments to the VoicePacketzier
    // Stack API. In this demo, each structure is 'filled in' by a specific function.

    struct TUserPCMSetupInfo UserPCMInfo;                                        /* The PCM configuration */
    struct acTCallProgressTone UserCallProgressTones[MAX_CALL_PROGRESS_SIGNALS]; /* Array of call-progress tones */
    TUserChannelSetupInfo UserChannelSetupInfo[MAX_CHANNELS_CAPACITY];           /* Array of channel configurations */
    int NumberOfCallProgressTones;                                               /* Number of defined call progress tones */
    int ChannelID;                                                               /* The current channel ID */
    int StackVersionNumber;                                                      /* The VoicePacketizer stack version nunber */

    struct TUserAGCSetupInfo UserAGCInfo = {20, 15, 3};
    struct TUserPatternDetectorSetupInfo UserPatternDetectorSetupInfo = {22, 33, 44, 55, 1};
    struct TJitterBufStatusReportParams JitterBufStatusReportParams = {0, 100};
    struct TExtendedVoiceParams ExtVoiceParams = {0}; //GetExtVoiceParams();

    VP_FUNC_ENT();

    VP_DBG("Init board...\r\n");
    if (InitializeEvaluationBoard() == -1)
        ErrorHandler(FatalErrorMsg, -1, __FILE__, __LINE__, acUnknownOrigin, -1, -1,
                     "Error initializing evaluation board!");
    VP_DBG("Init board done.\r\n");

#if (VP_FAST_INIT)
    usleep(5000);
#else
    usleep(500000);
#endif

    // Fill in the PCM configuration
    vp_ConfigureUserPCMSetupInfo(&UserPCMInfo);

    // Fill in the Call Progress Tones
    NumberOfCallProgressTones = vp_ConfigureCallProgressTone(UserCallProgressTones);

    // Fill in all the configuration of all channels
    for (ChannelID = 0; ChannelID < MAX_CHANNELS_CAPACITY; ChannelID++)
        vp_ConfigureUserChannelSetupInfo(&UserChannelSetupInfo[ChannelID], ChannelID);

    VP_DBG("Starting the VoicePacketizer Stack ...\n");

    // Start the VoicePacketizer stack
    // This convinient function performs all device and channel initializations and setup.
    StackVersionNumber = vpOpenStack(
        AC48X_NUM_OF_DEVICES,
#if (AC48X_DEVICE_TYPE == AC4880X_DEVICE)
        1, // dsp template version
#else
        0, // dsp template version
#endif
        0, // modify memory map
        NULL,
        0,
        UserPCMInfo,
        &UserChannelSetupInfo[0],
        NumberOfCallProgressTones,
        UserCallProgressTones,
        &UserAGCInfo,
        &UserPatternDetectorSetupInfo,
        &JitterBufStatusReportParams,
        ExtVoiceParams);

    VP_DBG("VoicePacketizer version is %d.%d.\n\n", StackVersionNumber / 100, StackVersionNumber % 100);

    /*
    VP_DBG("Connecting channels 0 & 1.\n\n");
    vp_ConnectChannels(&UserChannelSetupInfo[0], 0, &UserChannelSetupInfo[1], 1);
    */

    vp_OpenDefaultChannel(&UserChannelSetupInfo[0], VP_DEFAULT_CID);

    /* second time init as fixed by airbridge */
    InitializeEvaluationBoard();

    VP_FUNC_LEV();
}

void vp_Deinit_DSP()
{
    VP_FUNC_ENT();

    vpCloseStack();

    VP_FUNC_LEV();
}

void vp_TestVoiceStream()
{
    VP_FUNC_ENT();

    VP_DBG("Starting voice streaming ...\n\n");
    // Run the polling process in an infinite loop
    vp_PollingVoiceStream();

    VP_DBG("Demo ended.\n");
    while (1)
        ;

    VP_FUNC_LEV();
}

void vp_PollingVoiceStream()
{
    VP_FUNC_ENT();

    // Invoke the polling process (from DSP)
    vpPollingProcess(AC48X_NUM_OF_DEVICES);
    // Invoke the polling process (from FIFO).
    vpPollingMediaStreamFifo();

    /* Sleep(5); */
    //usleep(5000);

    VP_FUNC_LEV();
}

void vp_RTPDecode(char *FramePtr, int FrameLen)
{
    VP_FUNC_ENT();

    RTPDecoder(VP_DEFAULT_CID, FramePtr, FrameLen);

    VP_FUNC_LEV();
}

static void vp_ConfigureUserPCMSetupInfo(struct TUserPCMSetupInfo *UserPCMInfo)
{
    VP_FUNC_ENT();

    UserPCMInfo->PCMLawSelect = (AC48XIF_TYPE != AC48XIF_WINDOWS_PCI_ADB2255_DRIVER) ? UsedPcmLaw : ALaw; // 1=alaw 3=mulaw
    UserPCMInfo->E1_T1 = 0;                                                                               // 0=E1 (32 DS0), 1=T1 (24 DS0)
    UserPCMInfo->FSP = 0;                                                                                 // 0=FS active high, 1=FS active low
    UserPCMInfo->CLKP = 0;                                                                                // 0=data sampled on clk falling edge. 1=data sampled on clk rising edge.
    UserPCMInfo->IdleABCD = 0x8;                                                                          // ABCD CAS value transmitted at initialization
    UserPCMInfo->TriStateEnable = 0;                                                                      // =1 sets DSP PCM output to 3state between the run command and activation
    UserPCMInfo->IdlePCMPattern = (AC48XIF_TYPE != AC48XIF_WINDOWS_PCI_ADB2255_DRIVER) ? IdlePcm : (-1);  /* idle code to be injected to PCM bus until the decoder receives first packet, -1 to use default codes*/
    UserPCMInfo->ClockDirection = 1;                                                                      /* 0=Input/ 1=Output (AC483x & AC488x only) */
    UserPCMInfo->ClockDivider = (AC48X_DEVICE_TYPE == AC4880X_DEVICE) ? 21 : 47;                          // The PCM clock divider (relevant only if ClockDirection=0)
    UserPCMInfo->FrameWidth = 0;                                                                          // PCM frame synch width [PCM clock cycles]
    UserPCMInfo->ReverseSignalingAndVoicePorts = 0;                                                       // CAS/Voice Port Select
    UserPCMInfo->CPUClockOutEnable = 0;                                                                   // CPU Clock Out output pin (0-disable, 1-enable)
    UserPCMInfo->PCMDataDelay = 0;                                                                        // The beginning of actual PCM data reception or transmission with respect to the start of the frame sync signal.

    VP_FUNC_LEV();
}

/*
 * Defines call progress tones
 */
static int vp_ConfigureCallProgressTone(struct acTCallProgressTone *UserCallProgressTones)
{
    VP_FUNC_ENT();

    // CallProgress Tones:
    UserCallProgressTones[0].ToneType = acCallProgressDialTone; //the "logical" tone type
    UserCallProgressTones[0].ModulationType = acModulationNone;
    UserCallProgressTones[0].ToneForm = acDefaultToneForm;
    memset(&(UserCallProgressTones[0].u.AMTone), 0, sizeof(struct acTAMTone));

    UserCallProgressTones[0].u.Components[0].Frequency = 350;     //[Hz] freq of the lower component of the daul tone
    UserCallProgressTones[0].u.Components[1].Frequency = 440;     //[Hz] freq of the higher component of the daul tone
    UserCallProgressTones[0].u.Components[0].ComponentLevel = 19; //[-dBm] generation level of the lower component of the daul tone
    UserCallProgressTones[0].u.Components[1].ComponentLevel = 19; //[-dBm] generation level of the higher component of the daul tone
    UserCallProgressTones[0].ToneCadences[0].TOnDuration = 500;   //[10msec] first signal on time interval
    UserCallProgressTones[0].ToneCadences[0].TOffDuration = 0;    //[10msec] first signal off time interval
    UserCallProgressTones[0].ToneCadences[1].TOnDuration = 0;     //[10msec] second signal on time interval
    UserCallProgressTones[0].ToneCadences[1].TOffDuration = 0;    //[10msec] second signal off time interval

    VP_FUNC_LEV();

    // Return the number of defined call progress tones.
    return (1);
}

/*
 * Configures all the channel parameters.
 */
static void vp_ConfigureUserChannelSetupInfo(TUserChannelSetupInfo *UserChannelSetupInfo, int ChannelID)
{
    VP_FUNC_ENT();

    // Set the channel configuration array to its default values:
    SetChannelDefaults(UserChannelSetupInfo, ChannelID, 0);

    /* set to pcmu to fit twilio */
    UserChannelSetupInfo->VoiceCmd.Coder = G711Mulaw;

    VP_FUNC_LEV();
}

/*
 * Open & cross connect channels CID1 & CID2
 */
static void vp_ConnectChannels(TUserChannelSetupInfo *UserChannelInfo1, int CID1, TUserChannelSetupInfo *UserChannelInfo2, int CID2)
{
    VP_FUNC_ENT();

    // Activate CID1 and connect it to CID2
    UserChannelInfo1->Active = 1;
    UserChannelInfo1->RTPActive = 1;
    UserChannelInfo1->TxCID = CID2;
    vpOpenChannel(CID1, UserChannelInfo1);

    // Activate CID2 and connect it to CID1
    UserChannelInfo2->Active = 1;
    UserChannelInfo2->RTPActive = 1;
    UserChannelInfo2->TxCID = CID1;
    vpOpenChannel(CID2, UserChannelInfo2);

    VP_FUNC_LEV();
}

/*
 * Open default channel
 */
static void vp_OpenDefaultChannel(TUserChannelSetupInfo *UserChannelInfo1, int CID1)
{
    VP_FUNC_ENT();

    UserChannelInfo1->Active = 1;
    UserChannelInfo1->RTPActive = 1;
    UserChannelInfo1->TxCID = CID1;
    vpOpenChannel(CID1, UserChannelInfo1);

    VP_FUNC_LEV();
}
