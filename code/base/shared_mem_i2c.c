/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
         * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
       * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

