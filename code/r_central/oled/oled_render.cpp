#include "oled_render.h"
#include "oled_icon_loader.h"
#include "oled_ssd1306.h"
#include "../../base/hardware_i2c.h"
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/hw_procs.h"
#include "../shared_vars.h"

#include <pthread.h>
#include <string.h>
#include <iostream>
#include <stdexcept>

int s_iOLEDRenderInit = -1;
pthread_t s_pThreadOLEDRenderAsync;
bool s_bHasOLEDRenderThread = false;
t_i2c_device_settings *g_pDeviceInfoOLED = NULL;
u32 s_uOLDERRenderLastTime = 0;

void *_thread_oled_render_async(void *argument)
{
    log_line("[OLED] Started OLED render thread.");
    s_bHasOLEDRenderThread = true;
    OLEDIconLoader &loader = OLEDIconLoader::get_instance();
    loader.load_icons("res/");

    uint8_t test_count = 0;

    //load the bitmap picture which put in res/icon_oled_xxxxx.bmp
    //only support 1 bit color bmp file
    //you can use img2lcd to convert normal pjpg to bmp,and set the resolution above 128*64
    const OLEDIcon &icon_logo = loader.get_icon("logo");
    bool firstFrame = true;
    while ( ! g_bQuit )
    {
        test_count = test_count > 100 ? 0 : test_count;
        // control the frame rate less than 30fps
        u32 timestamp = get_current_timestamp_ms();
        u32 delta = timestamp - s_uOLDERRenderLastTime;
        if (delta < 30)
        {
            hardware_sleep_ms(30 - delta);
            continue;
        }
        s_uOLDERRenderLastTime = timestamp;

        ssd1306_oled_clear();
        ///////////////////////////////////////////////////////////////
        if (firstFrame)
        {
            firstFrame = false;

            ssd1306_oled_draw_bitmap(64, 24, 1, true, true, icon_logo);
            ssd1306_oled_draw_rect(34, 53, 60, 8, 3, 1, -1);
            for(int i = 0;i< 20;i++){
                ssd1306_oled_draw_rect(34, 53, i * 3, 8, 3, 1, 1);
                ssd1306_oled_display();
            }
           
            // vertical scroll
            // ssd1306_oled_vertical_right_horizontal_scroll(0, 0, 0x3F, SSD1306_SCROLL_FRAME_5);
            // horizontal scroll
            // ssd1306_oled_vertical_right_horizontal_scroll(0, 7, 0x00, SSD1306_SCROLL_FRAME_5);

            //hardware_sleep_ms(800);
            hardware_sleep_ms(500);
            
            continue;
        }

        //  Point triangle[3] = {
        //     {64, 10},
        //     {100, 50},
        //     {40, 50}};
        //  ssd1306_oled_draw_polygon(triangle, 3, 1, 0);
        //  ssd1306_oled_draw_ellipse(64, 32, 107, 43, 1, 0);
        //  for (int y_pos = 0; y_pos < 64; y_pos++)
        //{
        //      ssd1306_oled_draw_point(0, y_pos, 1);
        //      ssd1306_oled_draw_point(126, y_pos, 1);
        //  }
        //  for (int x_pos = 0; x_pos < 128; x_pos++)
        //{
        //      ssd1306_oled_draw_point(x_pos, 0, 1);
        //      ssd1306_oled_draw_point(x_pos, 63, 1);
        //  }

  
        ssd1306_oled_draw_rect(0, 0, 128, 64, 4, 1, -1);
        ssd1306_oled_draw_rect(3, 53, (120 * (test_count / 100.0)), 8, 3, 1, 1);
        ssd1306_oled_draw_rect(3, 53, 120, 8, 3, 1, -1);

        ssd1306_oled_draw_sector(27, 27, 25, 270, (360 * (test_count / 100.0) + 270), 1, 1);
        ssd1306_oled_draw_circle(27, 27, 25, 1, -1);

        // ssd1306_oled_draw_arc(20, 42, 22, 270, 33, 1);
        // ssd1306_oled_draw_arc(20, 42, 22, 0, 33, 1);
        char timestamp_str[30];
        int length = snprintf(timestamp_str, sizeof(timestamp_str), "%u ms", delta);
        ssd1306_oled_draw_string(100, 2, timestamp_str, length, 1, false, SSD1306_FONT_12);
        length = snprintf(timestamp_str, sizeof(timestamp_str), "%d %%", test_count);
        ssd1306_oled_draw_string(27, 27, timestamp_str, length, 1, true, SSD1306_FONT_12);
        ssd1306_oled_draw_string(54, 2, "Ruby FPV", 8, 1, false, SSD1306_FONT_12);
        ssd1306_oled_draw_string(54, 13, "Ruby FPV", 8, 1, false, SSD1306_FONT_16);
        ssd1306_oled_draw_string(54, 27, "Ruby FPV", 8, 1, false, SSD1306_FONT_24);
        test_count++;
        ///////////////////////////////////////////////////////////////
        ssd1306_oled_display();
    }

    log_line("[OLED] Finished OLED render thread.");
    s_bHasOLEDRenderThread = false;
    return NULL;
}

int oled_render_init()
{
   if ( s_iOLEDRenderInit != -1 )
      return s_iOLEDRenderInit;
   g_pDeviceInfoOLED = hardware_i2c_get_device_settings(I2C_DEVICE_ADDRESS_SSD1306_1);
   if ( (NULL == g_pDeviceInfoOLED) || (!g_pDeviceInfoOLED->bEnabled))
   {
       g_pDeviceInfoOLED = hardware_i2c_get_device_settings(I2C_DEVICE_ADDRESS_SSD1306_2);
   }

   if ( (NULL == g_pDeviceInfoOLED) || (!g_pDeviceInfoOLED->bEnabled))
       return -1;

   s_iOLEDRenderInit = ssd1306_oled_init(SSD1306_INTERFACE_IIC, g_pDeviceInfoOLED->nI2CAddress);
   return s_iOLEDRenderInit;
}

void oled_render_shutdown()
{
   if ( (-1 == s_iOLEDRenderInit) || (! s_bHasOLEDRenderThread) )
      return;

   u32 uTimeStart = get_current_timestamp_ms();
   while ( s_bHasOLEDRenderThread && (get_current_timestamp_ms() < uTimeStart + 1000) )
   {
      hardware_sleep_ms(20);
   }
   if ( s_bHasOLEDRenderThread )
      pthread_cancel(s_pThreadOLEDRenderAsync);
}

int oled_render_thread_start()
{
    if ( s_iOLEDRenderInit == -1 )
        return -1;

   s_bHasOLEDRenderThread = false;
    pthread_attr_t attr;
    struct sched_param params;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    params.sched_priority = 10;
    pthread_attr_setschedparam(&attr, &params);
    if (0 != pthread_create(&s_pThreadOLEDRenderAsync, &attr, &_thread_oled_render_async, NULL))
    {
        pthread_attr_destroy(&attr);
        log_softerror_and_alarm("[OLED] Failed to start oled render thread.");
        return -1;
    }
    pthread_attr_destroy(&attr);
    s_bHasOLEDRenderThread = true;
    return 0;
}