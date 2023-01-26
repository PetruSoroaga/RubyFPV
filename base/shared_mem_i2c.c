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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "shared_mem.h"
#include "shared_mem_i2c.h"

t_shared_mem_i2c_current* shared_mem_i2c_current_open_for_write()
{
   void *retVal = open_shared_mem(SHARED_MEM_NAME_I2C_CURRENT, sizeof(t_shared_mem_i2c_current), 0);
   t_shared_mem_i2c_current *pRetval = (t_shared_mem_i2c_current*)retVal;
   return pRetval;
}

t_shared_mem_i2c_current* shared_mem_i2c_current_open_for_read()
{
   void *retVal = open_shared_mem(SHARED_MEM_NAME_I2C_CURRENT, sizeof(t_shared_mem_i2c_current), 1);
   t_shared_mem_i2c_current *pRetval = (t_shared_mem_i2c_current*)retVal;
   return pRetval;
}

void shared_mem_i2c_current_close(t_shared_mem_i2c_current* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(t_shared_mem_i2c_current));
   //shm_unlink(SHARED_MEM_RX_STATS);
}


t_shared_mem_i2c_controller_rc_in* shared_mem_i2c_controller_rc_in_open_for_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN, sizeof(t_shared_mem_i2c_controller_rc_in), 1);
   t_shared_mem_i2c_controller_rc_in *tretval = (t_shared_mem_i2c_controller_rc_in*)retVal;
   return tretval;
}

t_shared_mem_i2c_controller_rc_in* shared_mem_i2c_controller_rc_in_open_for_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN, sizeof(t_shared_mem_i2c_controller_rc_in), 0);
   t_shared_mem_i2c_controller_rc_in *tretval = (t_shared_mem_i2c_controller_rc_in*)retVal;
   return tretval;
}

void shared_mem_i2c_controller_rc_in_close(t_shared_mem_i2c_controller_rc_in* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(t_shared_mem_i2c_controller_rc_in));
   //shm_unlink(SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN);
}

t_shared_mem_i2c_rotary_encoder_buttons_events* shared_mem_i2c_rotary_encoder_buttons_events_open_for_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_NAME_I2C_ROTARY_ENCODER_BUTTONS_EVENTS, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events), 1);
   return ((t_shared_mem_i2c_rotary_encoder_buttons_events*)retVal);
}

t_shared_mem_i2c_rotary_encoder_buttons_events* shared_mem_i2c_rotary_encoder_buttons_events_open_for_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_NAME_I2C_ROTARY_ENCODER_BUTTONS_EVENTS, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events), 0);
   return ((t_shared_mem_i2c_rotary_encoder_buttons_events*)retVal);
}

void shared_mem_i2c_rotary_encoder_buttons_events_close(t_shared_mem_i2c_rotary_encoder_buttons_events* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events));
}

