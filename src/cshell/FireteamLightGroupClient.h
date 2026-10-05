#ifndef __FIRETEAM_LIGHTGROUP_CLIENT_H__
#define __FIRETEAM_LIGHTGROUP_CLIENT_H__

#include <ltbasetypes.h>
#include <ltvector.h>

void FT_QueueLightGroup(uint32 nID, const LTVector &vAdjustment);
void FT_UpdateLightGroups();
void FT_ClearLightGroups();

#endif