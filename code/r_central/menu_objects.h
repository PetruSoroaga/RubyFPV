#pragma once
#include "menu_items.h"
#include "menu_item_select.h"
#include "../base/base.h"

#include "handle_commands.h"

#define MENU_ID_SIMPLE_MESSAGE 0
#define MENU_ID_ROOT 1
#define MENU_ID_SEARCH 2
#define MENU_ID_SYSTEM 3
#define MENU_ID_CONTROLLER 4
#define MENU_ID_VEHICLES 5
#define MENU_ID_SPECTATOR 6
#define MENU_ID_PREFERENCES 7
#define MENU_ID_STORAGE 11
#define MENU_ID_CONFIRMATION 12

#define MENU_ID_VEHICLE 20
#define MENU_ID_VEHICLE_GENERAL 21
#define MENU_ID_VEHICLE_RADIO 22
#define MENU_ID_VEHICLE_VIDEO 23
#define MENU_ID_VEHICLE_CAMERA 24
#define MENU_ID_VEHICLE_OSD 25
#define MENU_ID_VEHICLE_EXPERT 26
#define MENU_ID_VEHICLE_TELEMETRY 27
#define MENU_ID_VEHICLE_EXPERT_ENCODINGS 28
#define MENU_ID_VEHICLE_RC 29
#define MENU_ID_VEHICLE_RELAY 30
#define MENU_ID_VEHICLE_ALARMS 31
#define MENU_ID_VEHICLE_CAMERA_GAINS 35
#define MENU_ID_CONTROLLER_PERIPHERALS 38
#define MENU_ID_CONTROLLER_EXPERT 39
#define MENU_ID_CONTROLLER_CPU 40
#define MENU_ID_VEHICLE_CPU 42
#define MENU_ID_VEHICLE_RADIO_INTERFACES 44
#define MENU_ID_CONTROLLER_RADIO_INTERFACES 45
#define MENU_ID_OSD_AHI 46
#define MENU_ID_PREFERENCES_UI 47
#define MENU_ID_SYSTEM_EXPERT 48
#define MENU_ID_VEHICLE_MANAGEMENT 49
#define MENU_ID_VEHICLE_IMPORT 50
#define MENU_ID_SWITCH_VEHICLE 55
#define MENU_ID_CONTROLLER_JOYSTICK 56
#define MENU_ID_SYSTEM_ALL_PARAMS 57
#define MENU_ID_VEHICLE_RC_CHANNELS 58
#define MENU_ID_COLOR_PICKER 59
#define MENU_ID_VEHICLE_RC_FAILSAFE 60
#define MENU_ID_VEHICLE_RC_EXPO 61
#define MENU_ID_CONTROLLER_VIDEO 62
#define MENU_ID_CONTROLLER_TELEMETRY 63
#define MENU_ID_UPDATE_VEHICLE_POPUP 64
#define MENU_ID_VEHICLE_RC_CAMERA 65
#define MENU_ID_VEHICLE_RADIO_INTERFACES_EXPERT 67
#define MENU_ID_DEVICE_I2C 68
#define MENU_ID_VEHICLE_OSD_STATS 69
#define MENU_ID_VEHICLE_AUDIO 70
#define MENU_ID_VEHICLE_DATA_LINK 71
#define MENU_ID_CONTROLLER_NETWORK 72
#define MENU_ID_VEHICLE_OSD_PLUGINS 73
#define MENU_ID_VEHICLE_INSTRUMENTS_GENERAL 74
#define MENU_ID_VEHICLE_OSD_ELEMENTS 75
#define MENU_ID_VEHICLE_OSD_PLUGIN 76
#define MENU_ID_CONTROLLER_PLUGINS 77
#define MENU_ID_CONTROLLER_ENCRYPTION 78
#define MENU_ID_SEARCH_CONNECT 79
#define MENU_ID_SYSTEM_HARDWARE 80
#define MENU_ID_VEHICLE_RC_INPUT 81
#define MENU_ID_CONFIRMATION_HDMI 82
#define MENU_ID_CONTROLLER_RECORDING 83
#define MENU_ID_SYSTEM_VIDEO_PROFILES 84
#define MENU_ID_VEHICLE_VIDEO_PROFILE 85
#define MENU_ID_VEHICLE_FUNCTIONS 86
#define MENU_ID_CHANNELS_SELECT 87
#define MENU_ID_INFO_BOOSTER 88
#define MENU_ID_TX_POWER_MAX 89
#define MENU_ID_DEV_LOGS 90
#define MENU_ID_CALIBRATE_HDMI 91
#define MENU_ID_SYSTEM_ALARMS 92
#define MENU_ID_RADIO_CONFIG 93
#define MENU_ID_CONTROLLER_RADIO_INTERFACE 94
#define MENU_ID_SYSTEM_DEV_STATS 95
#define MENU_ID_VEHICLE_RADIO_LINK 96
#define MENU_ID_VEHICLE_RADIO_INTERFACE 97
#define MENU_ID_VEHICLE_MANAGE_PLUGINS 98
#define MENU_ID_VEHICLE_PERIPHERALS 99
#define MENU_ID_VEHICLE_RADIO_LINK_SIK 100
#define MENU_ID_CONTROLLER_RADIO_INTERFACE_SIK 101
#define MENU_ID_VEHICLE_SELECTOR 102

#define MENU_ID_TEXT 80
#define MENU_ID_TXINFO 81


#define MAX_MENU_ITEMS 150

extern float MENU_TEXTLINE_SPACING; // percent of item text height
extern float MENU_ITEM_SPACING;  // percent of item text height

#define MENU_FONT_SIZE_TITLE 0.023
#define MENU_FONT_SIZE_SUBTITLE 0.019
#define MENU_FONT_SIZE_ITEMS 0.021
#define MENU_FONT_SIZE_TOPLINE 0.02
#define MENU_FONT_SIZE_TOOLTIPS 0.018
#define MENU_MARGINS 0.028
#define MENU_ROUND_MARGIN 0.02
#define MENU_SELECTION_PADDING 0.006
#define MENU_SEPARATOR_HEIGHT 0.6 // percent of item text height
#define MENU_OUTLINEWIDTH 0.0

#define MENU_ALFA_WHEN_IN_BG 0.3

#define MENU_MAX_TOP_LINES 40

class Menu
{
   public:
     Menu(int id, const char* title, const char* subTitle);
     virtual ~Menu();
     float m_xPos; float m_yPos;
     float m_xPosOriginal;
     float m_Width; float m_Height;
     char m_szTitle[256];
     char m_szSubTitle[256];
     char m_szCurrentTooltip[512];
     char* m_szTopLines[MENU_MAX_TOP_LINES];
     float m_fTopLinesDX[MENU_MAX_TOP_LINES];
     int m_TopLinesCount;
     int m_MenuId;
     int m_MenuDepth;
     float m_fAlfaWhenInBackground;

     void setParent(Menu* pParent);
     void invalidate();
     void disableScrolling();
     void setTitle(const char* szTitle);
     void setSubTitle(const char* szSubTitle);
     void removeAllTopLines();
     void addTopLine(const char* szLine, float dx=0.0f);
     void setColumnsCount(int count);
     void enableColumnSelection(bool bEnable);
     void removeAllItems();
     int addMenuItem(MenuItem* item);
     void addSeparator();
     void enableMenuItem(int index, bool enable);
     void setTooltip(const char* szTooltip);
     void setTopIcon(const char* szIcon);
     u32 getOnShowTime();
     u32 getOnChildAddTime();
     u32 getOnReturnFromChildTime();

     virtual bool periodicLoop();
     virtual float RenderFrameAndTitle();
     virtual float RenderItem(int index, float yPos, float dx = 0);
     virtual void RenderPrepare();
     virtual void Render();
     virtual void RenderEnd(float yTop);
     virtual void onShow();
     virtual int  onBack();
     virtual void onSelectItem();
     virtual void onMoveUp(bool bIgnoreReversion);
     virtual void onMoveDown(bool bIgnoreReversion);
     virtual void onMoveLeft(bool bIgnoreReversion);
     virtual void onMoveRight(bool bIgnoreReversion);
     virtual void onFocusedItemChanged();
     virtual void onItemValueChanged(int itemIndex);
     virtual void onItemEndEdit(int itemIndex);
     virtual void onChildMenuAdd();
     virtual void onReturnFromChild(int returnValue);
     virtual void valuesToUI(){};

     float getSelectionWidth();
     float getUsableWidth();
     float getRenderWidth();
     int getConfirmationId();

   protected:
     void computeRenderSizes();
     void updateScrollingOnSelectionChange();
     bool checkIsArmed();
     void addMessage(const char* szMessage);
     void addMessageNeedsVehcile(const char* szMessage, int iConfirmationId);
     bool uploadSoftware();
     bool _uploadVehicleUpdate(int iUpdateType, const char* szArchiveToUpload);
     bool checkCancelUpload();

     MenuItemSelect* createMenuItemCardModelSelector(const char* szName);
     
     bool m_bInvalidated;
     bool m_bHasChildMenuActive;
     bool m_bFullWidthSelection;
     bool m_bFirstShow;
     Menu* m_pParent;
     MenuItem* m_pMenuItems[MAX_MENU_ITEMS];
     bool m_bHasSeparatorAfter[MAX_MENU_ITEMS];
     int m_ItemsCount;
     int m_iColumnsCount;
     bool m_bEnableColumnSelection;
     bool m_bIsSelectingInsideColumn;
     int m_SelectedIndex;
     float m_fSelectionWidth;
     float m_ExtraHeight;
     float m_RenderXPos;
     float m_RenderYPos;
     float m_RenderTotalHeight;
     float m_RenderHeight;
     float m_RenderWidth;
     float m_RenderHeaderHeight;
     float m_RenderTitleHeight;
     float m_RenderSubtitleHeight;
     float m_RenderFooterHeight;
     float m_RenderTooltipHeight;
     float m_RenderMaxFooterHeight;
     float m_RenderMaxTooltipHeight;
     float m_fRenderScrollOffsetY;
     float m_fRenderScrollMaxY;
     float m_fRenderScrollBarsExtraWidth;
     float m_fRenderItemsStartYPos;
     float m_fRenderItemsMaxHeight;
     bool  m_bEnableScrolling;
     bool  m_bHasScrolling;

     char m_szTopIcon[6];
     float m_fIconSize;

     u32 m_uOnShowTime;
     u32 m_uOnChildAddTime;
     u32 m_uOnChildCloseTime;
     u32 m_bAnimating;
     int m_iConfirmationId;
     bool m_bHasConfirmationOnTop;
};
