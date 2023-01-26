#pragma once

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"

void rotatePoints(RenderEngineUI* pEngine, float *px, float *py, float angle, int points, float center_x, float center_y, float fScale);
void rotate_point(RenderEngineUI* pEngine, float x, float y, float xCenter, float yCenter, float angle, float* px, float* py);
float show_value_centered(RenderEngineUI* pEngine, float x, float y, const char* szValue, u32 fontId);
void draw_shadow(RenderEngineUI* pEngine, float xCenter, float yCenter, float fRadius);
