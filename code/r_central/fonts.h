#pragma once

extern u32 g_idFontOSD;
extern u32 g_idFontOSDBig;
extern u32 g_idFontOSDSmall;
extern u32 g_idFontOSDExtraSmall;
extern u32 g_idFontOSDWarnings;
extern u32 g_idFontStats;
extern u32 g_idFontStatsSmall;
extern u32 g_idFontMenu;
extern u32 g_idFontMenuSmall;
extern u32 g_idFontMenuLarge;

bool loadAllFonts(bool bReloadMenuFonts);
void free_all_fonts();

void applyFontScaleChanges();
