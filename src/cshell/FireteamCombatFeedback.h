#ifndef __FIRETEAM_COMBAT_FEEDBACK_H__
#define __FIRETEAM_COMBAT_FEEDBACK_H__

class ILTMessage_Read;

void FT_CombatFeedbackInit();
void FT_CombatFeedbackTerm();
void FT_CombatFeedbackHandleMessage(
    ILTMessage_Read *pMessage);
void FT_CombatFeedbackShowRoundStart();
void FT_RenderCombatFeedback();

#endif
