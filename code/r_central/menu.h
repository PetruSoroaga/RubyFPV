#pragma once
#include "menu_objects.h"

#define MAX_MENU_STACK 20

float menu_getScaleMenus();
void menu_setScaleMenus(float f);
float menu_get_XStartPos(float fWidth);

float menu_setGlobalAlpha(float fAlpha);
float menu_getGlobalAlpha();

int menu_init();
void menu_loop();
void menu_refresh_all_menus();
void menu_render();
void menu_close_all();
void add_menu_to_stack(Menu* pMenu);
void remove_menu_from_stack(Menu* pMenu);
void replace_menu_on_stack(Menu* pMenuSrc, Menu* pMenuNew);

void menu_stack_pop(int returnValue = 0);
bool menu_is_menu_on_top(Menu* pMenu);
bool menu_has_menu_confirmation_above(Menu* pMenu);
bool menu_has_menu(int menuId);
bool isMenuOn();

void menu_invalidate_all();
void apply_Preferences();

bool menu_check_current_model_ok_for_edit();
bool menu_has_animation_in_progress();
