/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "popup_commands.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"

PopupCommands::PopupCommands()
:PopupLog()
{
   setTitle("Commands");
}

PopupCommands::~PopupCommands()
{
}

