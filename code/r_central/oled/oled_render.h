#pragma once
#include "../../base/base.h"
#include "../../base/config.h"
#include "oled_ssd1306.h"

int oled_render_init();
void oled_render_shutdown();
int oled_render_thread_start();