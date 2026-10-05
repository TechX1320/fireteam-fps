#ifndef __FIRETEAM_SETTINGS_MENU_H__
#define __FIRETEAM_SETTINGS_MENU_H__

#include <ltbasedefs.h>

class CUIFont;

void FT_SettingsInit();
void FT_SettingsTerm();
void FT_SettingsRender();
void FT_SettingsToggle();
bool FT_SettingsIsOpen();
bool FT_SettingsHandleKey(int nKey);

#endif