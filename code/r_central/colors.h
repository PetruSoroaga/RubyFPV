#pragma once

const double* get_Color_Dev();

const double* get_Color_OSDBackground();
const double* get_Color_OSDText();
const double* get_Color_OSDTextOutline();
const double* get_Color_OSDWarning();
const double* get_Color_OSDChangedValue();

const double* get_Color_PopupBg();
const double* get_Color_PopupBorder();
const double* get_Color_PopupText();

const double* get_Color_MenuBg();
const double* get_Color_MenuBgTitle();
const double* get_Color_MenuBgTooltip();
const double* get_Color_MenuBorder();
const double* get_Color_MenuText();
const double* get_Color_MenuItemSelectedBg();
const double* get_Color_MenuItemSelectedText();
const double* get_Color_MenuItemDisabledText();

const double* get_Color_IconNormal();
const double* get_Color_IconWarning();
const double* get_Color_IconError();
const double* get_Color_IconSucces();

const double* get_Color_OSDElementChanged();


void set_Color_OSDText(float r, float g, float b, float a);
void set_Color_OSDOutline(float r, float g, float b, float a);

