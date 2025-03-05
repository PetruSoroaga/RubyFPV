#pragma once
#include "driver_ssd1306.h"
#include "oled_icon_loader.h"

#define SSD1306_OLED_DEFAULT_DESELECT_LEVEL SSD1306_DESELECT_LEVEL_0P77            /** set deselect level 0.77 */
#define SSD1306_OLED_DEFAULT_LEFT_RIGHT_REMAP SSD1306_LEFT_RIGHT_REMAP_DISABLE     /** disable remap */
#define SSD1306_OLED_DEFAULT_PIN_CONF SSD1306_PIN_CONF_ALTERNATIVE                 /** set alternative */
#define SSD1306_OLED_DEFAULT_PHASE1_PERIOD 0x01                                    /** set phase 1 */
#define SSD1306_OLED_DEFAULT_PHASE2_PERIOD 0x0F                                    /** set phase F */
#define SSD1306_OLED_DEFAULT_OSCILLATOR_FREQUENCY 0x08                             /** set 8 */
#define SSD1306_OLED_DEFAULT_CLOCK_DIVIDE 0x00                                     /** set clock div 0 */
#define SSD1306_OLED_DEFAULT_DISPLAY_OFFSET 0x00                                   /** set display offset */
#define SSD1306_OLED_DEFAULT_MULTIPLEX_RATIO 0x3F                                  /** set ratio */
#define SSD1306_OLED_DEFAULT_DISPLAY_MODE SSD1306_DISPLAY_MODE_NORMAL              /** set normal mode */
#define SSD1306_OLED_DEFAULT_SCAN_DIRECTION SSD1306_SCAN_DIRECTION_COMN_1_START    /** set scan 1 */
#define SSD1306_OLED_DEFAULT_SEGMENT SSD1306_SEGMENT_COLUMN_ADDRESS_127            /** set column 127 */
#define SSD1306_OLED_DEFAULT_CONTRAST 0xCF                                         /** set contrast CF */
#define SSD1306_OLED_DEFAULT_ZOOM_IN SSD1306_ZOOM_IN_DISABLE                       /** disable zoom in */
#define SSD1306_OLED_DEFAULT_FADE_BLINKING_MODE SSD1306_FADE_BLINKING_MODE_DISABLE /** disable fade */
#define SSD1306_OLED_DEFAULT_FADE_FRAMES 0x00                                      /** set frame 0 */
#define SSD1306_OLED_DEFAULT_DISPLAY_START_LINE 0x00                               /** set start line 0 */
#define SSD1306_OLED_DEFAULT_HIGH_COLUMN_START_ADDRESS 0x00                        /** set high start 0 */
#define SSD1306_OLED_DEFAULT_LOW_COLUMN_START_ADDRESS 0x00                         /** set low start 0 */
#define SSD1306_OLED_DEFAULT_PAGE_ADDRESS_RANGE_START 0x00                         /** set page range start */
#define SSD1306_OLED_DEFAULT_PAGE_ADDRESS_RANGE_END 0x07                           /** set page range end */
#define SSD1306_OLED_DEFAULT_COLUMN_ADDRESS_RANGE_START 0x00                       /** set range start */
#define SSD1306_OLED_DEFAULT_COLUMN_ADDRESS_RANGE_END 0x7F                         /** set range end */

int ssd1306_iic_init();
int ssd1306_iic_deinit();
int ssd1306_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
void ssd1306_delay_ms(uint32_t ms);
void ssd1306_debug_print(const char *const fmt, ...);
// int ssd1306_spi_init(void);
// int ssd1306_spi_deinit(void);
// int ssd1306_spi_write_cmd(uint8_t *buf, uint16_t len);
// int ssd1306_spi_cmd_data_gpio_init(void);
// int ssd1306_spi_cmd_data_gpio_deinit(void);
// int ssd1306_spi_cmd_data_gpio_write(uint8_t value);
// int ssd1306_reset_gpio_init(void);
// int ssd1306_reset_gpio_deinit(void);
// int ssd1306_reset_gpio_write(uint8_t value);

int ssd1306_oled_init(ssd1306_interface_t interface, int addr);

int ssd1306_oled_deinit(void);

int ssd1306_oled_display_on(void);

int ssd1306_oled_display_off(void);

int ssd1306_oled_clear(void);

int ssd1306_oled_clear_area(int16_t x, int16_t y, int16_t width, int16_t height);

int ssd1306_oled_display(void);

int ssd1306_oled_draw_point(int16_t x, int16_t y, uint8_t data);

int ssd1306_oled_get_point(int16_t x, int16_t y);

int ssd1306_oled_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);

int ssd1306_oled_draw_string(int16_t x, int16_t y, char *str, uint16_t len, uint8_t color, bool center, ssd1306_font_t font);

int ssd1306_oled_draw_rect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t corner, uint8_t color, int8_t fill_color);

int ssd1306_oled_draw_polygon(const Point *vertices, uint8_t num_vertices, uint8_t color, int8_t fill_color);

int ssd1306_oled_draw_circle(int16_t center_x, int16_t center_y, uint8_t radius, uint8_t color, int8_t fill_color);

int ssd1306_oled_draw_arc(int16_t center_x, int16_t center_y, uint8_t radius, uint16_t start_angle, uint16_t end_angle, uint8_t color);

int ssd1306_oled_draw_sector(int16_t center_x, int16_t center_y, uint8_t radius, uint16_t start_angle, uint16_t end_angle, int8_t color, int8_t fill_color);

int ssd1306_oled_draw_ellipse(int16_t center_x, int16_t center_y, uint16_t width, uint16_t height, uint8_t color, int8_t fill_color);

int ssd1306_oled_draw_bitmap(int16_t x, int16_t y, uint8_t color, bool center,bool transparent, const OLEDIcon &icon);

int ssd1306_oled_enable_zoom_in(void);

int ssd1306_oled_disable_zoom_in(void);

int ssd1306_oled_fade_blinking(ssd1306_fade_blinking_mode_t mode, uint8_t frames);

int ssd1306_oled_deactivate_scroll(void);

int ssd1306_oled_vertical_left_horizontal_scroll(uint8_t start_page_addr, uint8_t end_page_addr, uint8_t rows, ssd1306_scroll_frame_t frames);

int ssd1306_oled_vertical_right_horizontal_scroll(uint8_t start_page_addr, uint8_t end_page_addr, uint8_t rows, ssd1306_scroll_frame_t frames);