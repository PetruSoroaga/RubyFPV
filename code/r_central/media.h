#pragma once

bool media_init_and_scan();
void media_scan_files();
int media_get_screenshots_count();
int media_get_videos_count();

char* media_get_screenshot_filename();
char* media_get_video_filename();

bool media_take_screenshot(bool bIncludeOSD);

