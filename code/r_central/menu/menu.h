#pragma once
#include "menu_objects.h"
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_preferences.h"
#include "../../base/ctrl_settings.h"
#include "../../common/string_utils.h"
#include "../../renderer/render_engine.h"

#include "../../radio/radioflags.h"

#include "../pairing.h"
#include "../colors.h"
#include "../fonts.h"
#include "../local_stats.h"
#include "../shared_vars.h"
#include "../popup.h"
#include "../handle_commands.h"
#include "../ruby_central.h"
#include "../timers.h"
#include "../events.h"
#include "../notifications.h"
#include "../warnings.h"
#include "../process_router_messages.h"
#include "../ruby_central.h"

#define MAX_MENU_STACK 20

float menu_get_XStartPos(float fWidth);
float menu_get_XStartPos(Menu* pMenu, float fWidth);

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
Menu* menu_get_menu_by_id(int menuId);
bool isMenuOn();

void menu_invalidate_all();

void menu_startAnimationOnChildMenuAdd(Menu* pTopMenu);
void menu_startAnimationOnChildMenuClosed(Menu* pTopMenu);

bool menu_check_current_model_ok_for_edit();
bool menu_has_animation_in_progress();
