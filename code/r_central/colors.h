#pragma once

double* get_Color_Dev();

double* get_Color_OSDBackground();
double* get_Color_OSDText();
double* get_Color_OSDTextOutline();
double* get_Color_OSDWarning();
double* get_Color_OSDChangedValue();

double* get_Color_PopupBg();
double* get_Color_PopupBorder();
double* get_Color_PopupText();

double* get_Color_MenuBg();
double* get_Color_MenuBgTitle();
double* get_Color_MenuBgTooltip();
double* get_Color_MenuBorder();
double* get_Color_MenuText();
double* get_Color_MenuItemSelectedBg();
double* get_Color_MenuItemSelectedText();
double* get_Color_MenuItemDisabledText();

double* get_Color_IconNormal();
double* get_Color_IconWarning();
double* get_Color_IconError();
double* get_Color_IconSucces();

double* get_Color_OSDElementChanged();


void set_Color_OSDText(float r, float g, float b, float a);
void set_Color_OSDOutline(float r, float g, float b, float a);

