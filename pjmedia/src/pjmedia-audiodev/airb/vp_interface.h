int InitializeEvaluationBoard();
void vp_ConfigureUserPCMSetupInfo(struct TUserPCMSetupInfo *UserPCMInfo);
int vp_ConfigureCallProgressTone(struct acTCallProgressTone *UserCallProgressTones);
void vp_ConfigureUserChannelSetupInfo(TUserChannelSetupInfo *UserChannelSetupInfo, int ChannelID);
void vp_PollingVoiceStream(void);
void vp_ConnectChannels(TUserChannelSetupInfo *UserChannelInfo1, int CID1, TUserChannelSetupInfo *UserChannelInfo2, int CID2);
