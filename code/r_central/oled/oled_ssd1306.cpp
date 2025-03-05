#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "driver_ssd1306.h"
#include "oled_ssd1306.h"
#include "oled_icon_loader.h"
#include "../../base/base.h"
#include "../../base/hardware_i2c.h"

#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RASPBERRY)
#include <wiringPiI2C.h>
#endif
#if defined(HW_CAPABILITY_I2C) && defined(HW_PLATFORM_RADXA_ZERO3)
#include "../../base/wiringPiI2C_radxa.h"
#endif

int i2c_fd = 0;
static ssd1306_handle_t gs_handle;

int ssd1306_iic_init()
{
    i2c_fd = wiringPiI2CSetup(gs_handle.iic_addr);
    return 0;
}

int ssd1306_iic_deinit()
{
    return 0;
}

int ssd1306_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (len == 0)
        return 0;

#ifdef HW_PLATFORM_RASPBERRY
    if (len == 1)
    {
        return wiringPiI2CWriteReg8(i2c_fd, reg, *buf);
    }
    for (int i = 0; i < len; i++)
    {
        wiringPiI2CWriteReg8(i2c_fd, reg, buf[i]);
    }
    return 0;

#endif

    if (len == 1)
    {
        return wiringPiI2CWriteReg8(i2c_fd, reg, *buf);
    }
    else
    {
        return wiringPiI2CWriteBlockData(i2c_fd, reg, len, buf);
    }
}

void ssd1306_delay_ms(uint32_t ms)
{
    hardware_sleep_ms(ms);
}

void ssd1306_debug_print(const char *const fmt, ...)
{
    log_line(fmt);
}

// int ssd1306_spi_init(void)
//{
//     return 0;
// }

// int ssd1306_spi_deinit(void)
//{
//     return 0;
// }

// int ssd1306_spi_write_cmd(uint8_t *buf, uint16_t len)
//{
//     return 0;
// }

// int ssd1306_spi_cmd_data_gpio_init(void)
//{
//     return 0;
// }

// int ssd1306_spi_cmd_data_gpio_deinit(void)
//{
//     return 0;
// }

// int ssd1306_spi_cmd_data_gpio_write(uint8_t value)
//{
//     return 0;
// }

// int ssd1306_reset_gpio_init(void)
//{
//     return 0;
// }

// int ssd1306_reset_gpio_deinit(void)
//{
//     return 0;
// }

// int ssd1306_reset_gpio_write(uint8_t value)
//{
//     return 0;
// }

int ssd1306_oled_init(ssd1306_interface_t interface, int addr)
{
    int err = 0;

    DRIVER_SSD1306_LINK_INIT(&gs_handle, ssd1306_handle_t);
    DRIVER_SSD1306_LINK_IIC_INIT(&gs_handle, ssd1306_iic_init);
    DRIVER_SSD1306_LINK_IIC_DEINIT(&gs_handle, ssd1306_iic_deinit);
    DRIVER_SSD1306_LINK_IIC_WRITE(&gs_handle, ssd1306_iic_write);
    DRIVER_SSD1306_LINK_DELAY_MS(&gs_handle, ssd1306_delay_ms);
    DRIVER_SSD1306_LINK_DEBUG_PRINT(&gs_handle, ssd1306_debug_print);
    // DRIVER_SSD1306_LINK_SPI_INIT(&gs_handle, ssd1306_spi_init);
    // DRIVER_SSD1306_LINK_SPI_DEINIT(&gs_handle, ssd1306_spi_deinit);
    // DRIVER_SSD1306_LINK_SPI_WRITE_COMMAND(&gs_handle, ssd1306_spi_write_cmd);
    // DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_INIT(&gs_handle, ssd1306_spi_cmd_data_gpio_init);
    // DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_DEINIT(&gs_handle, ssd1306_spi_cmd_data_gpio_deinit);
    // DRIVER_SSD1306_LINK_SPI_COMMAND_DATA_GPIO_WRITE(&gs_handle, ssd1306_spi_cmd_data_gpio_write);
    // DRIVER_SSD1306_LINK_RESET_GPIO_INIT(&gs_handle, ssd1306_reset_gpio_init);
    // DRIVER_SSD1306_LINK_RESET_GPIO_DEINIT(&gs_handle, ssd1306_reset_gpio_deinit);
    // DRIVER_SSD1306_LINK_RESET_GPIO_WRITE(&gs_handle, ssd1306_reset_gpio_write);

    err |= ssd1306_set_interface(&gs_handle, interface);
    err |= ssd1306_set_addr(&gs_handle, addr);
    err |= ssd1306_init(&gs_handle);
    err |= ssd1306_set_display(&gs_handle, SSD1306_DISPLAY_OFF);
    err |= ssd1306_set_column_address_range(&gs_handle, SSD1306_OLED_DEFAULT_COLUMN_ADDRESS_RANGE_START, SSD1306_OLED_DEFAULT_COLUMN_ADDRESS_RANGE_END);
    err |= ssd1306_set_page_address_range(&gs_handle, SSD1306_OLED_DEFAULT_PAGE_ADDRESS_RANGE_START, SSD1306_OLED_DEFAULT_PAGE_ADDRESS_RANGE_END);
    err |= ssd1306_set_low_column_start_address(&gs_handle, SSD1306_OLED_DEFAULT_LOW_COLUMN_START_ADDRESS);
    err |= ssd1306_set_high_column_start_address(&gs_handle, SSD1306_OLED_DEFAULT_HIGH_COLUMN_START_ADDRESS);
    err |= ssd1306_set_display_start_line(&gs_handle, SSD1306_OLED_DEFAULT_DISPLAY_START_LINE);
    err |= ssd1306_set_fade_blinking_mode(&gs_handle, SSD1306_OLED_DEFAULT_FADE_BLINKING_MODE, SSD1306_OLED_DEFAULT_FADE_FRAMES);
    err |= ssd1306_deactivate_scroll(&gs_handle);
    err |= ssd1306_set_zoom_in(&gs_handle, SSD1306_OLED_DEFAULT_ZOOM_IN);
    err |= ssd1306_set_contrast(&gs_handle, SSD1306_OLED_DEFAULT_CONTRAST);
    err |= ssd1306_set_segment_remap(&gs_handle, SSD1306_OLED_DEFAULT_SEGMENT);
    err |= ssd1306_set_scan_direction(&gs_handle, SSD1306_OLED_DEFAULT_SCAN_DIRECTION);
    err |= ssd1306_set_display_mode(&gs_handle, SSD1306_OLED_DEFAULT_DISPLAY_MODE);
    err |= ssd1306_set_multiplex_ratio(&gs_handle, SSD1306_OLED_DEFAULT_MULTIPLEX_RATIO);
    err |= ssd1306_set_display_offset(&gs_handle, SSD1306_OLED_DEFAULT_DISPLAY_OFFSET);
    err |= ssd1306_set_display_clock(&gs_handle, SSD1306_OLED_DEFAULT_OSCILLATOR_FREQUENCY, SSD1306_OLED_DEFAULT_CLOCK_DIVIDE);
    err |= ssd1306_set_precharge_period(&gs_handle, SSD1306_OLED_DEFAULT_PHASE1_PERIOD, SSD1306_OLED_DEFAULT_PHASE2_PERIOD);
    err |= ssd1306_set_com_pins_hardware_conf(&gs_handle, SSD1306_OLED_DEFAULT_PIN_CONF, SSD1306_OLED_DEFAULT_LEFT_RIGHT_REMAP);
    err |= ssd1306_set_deselect_level(&gs_handle, SSD1306_OLED_DEFAULT_DESELECT_LEVEL);
    err |= ssd1306_set_memory_addressing_mode(&gs_handle, SSD1306_MEMORY_ADDRESSING_MODE_PAGE);
    err |= ssd1306_set_charge_pump(&gs_handle, SSD1306_CHARGE_PUMP_ENABLE);
    err |= ssd1306_set_entire_display(&gs_handle, SSD1306_ENTIRE_DISPLAY_OFF);
    err |= ssd1306_set_display(&gs_handle, SSD1306_DISPLAY_ON);
    err |= ssd1306_clear(&gs_handle, 0, 0, SSD1306_WIDTH, SSD1306_HEIGHT);
    err |= ssd1306_update(&gs_handle);

    return (err == 0) ? 0 : -1;
}

int ssd1306_oled_deinit(void)
{
    return ssd1306_deinit(&gs_handle);
}

int ssd1306_oled_display_on(void)
{
    return ssd1306_set_display(&gs_handle, SSD1306_DISPLAY_ON);
}

int ssd1306_oled_display_off(void)
{
    return ssd1306_set_display(&gs_handle, SSD1306_DISPLAY_OFF);
}

int ssd1306_oled_clear(void)
{
    return ssd1306_clear(&gs_handle, 0, 0, SSD1306_WIDTH, SSD1306_HEIGHT);
}

int ssd1306_oled_clear_area(int16_t x, int16_t y, int16_t width, int16_t height)
{

    return ssd1306_clear(&gs_handle, x, y, width, height);
}

int ssd1306_oled_display(void)
{
    return ssd1306_update(&gs_handle);
}

int ssd1306_oled_draw_point(int16_t x, int16_t y, uint8_t data)
{
    return ssd1306_draw_point(&gs_handle, x, y, data);
}

int ssd1306_oled_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        if (ssd1306_draw_point(&gs_handle, x1, y1, color) != 0)
        {
            return -1;
        }

        if (x1 == x2 && y1 == y2)
            break;

        // Bresenham
        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }

    return 0;
}

int ssd1306_oled_get_point(int16_t x, int16_t y)
{
    return ssd1306_get_point(&gs_handle, x, y);
}

// The function has been verified.
int ssd1306_oled_draw_string(int16_t x, int16_t y, char *str, uint16_t len, uint8_t color, bool center, ssd1306_font_t font)
{
    if (center)
    {
        uint16_t total_width = len * (font / 2);
        x = x - (total_width / 2);
        y = y - (font / 2);
    }
    return ssd1306_draw_string(&gs_handle, x, y, str, len, color, font);
}

// The function has been verified.
int ssd1306_oled_draw_rect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t corner, uint8_t color, int8_t fill_color)
{
    if (width == 0 || height == 0)
    {
        return -1;
    }

    if (corner > width / 2 || corner > height / 2)
    {
        return -1;
    }

    if (corner == 0)
    {
        int ret = ssd1306_draw_rect(&gs_handle, x, y, width, height, color);
        if (fill_color == -1)
            return ret;

        for (int16_t fill_x = x + 1; fill_x < x + width; fill_x++)
        {
            for (int16_t fill_y = y + 1; fill_y < y + height; fill_y++)
            {
                ssd1306_draw_point(&gs_handle, fill_x, fill_y, fill_color);
            }
        }
        return 0;
    }

    int16_t right_x = x + width - 1;
    int16_t bottom_y = y + height - 1;
    int16_t top_left_center_x = x + corner;
    int16_t top_left_center_y = y + corner;
    int16_t top_right_center_x = right_x - corner;
    int16_t top_right_center_y = y + corner;
    int16_t bottom_left_center_x = x + corner;
    int16_t bottom_left_center_y = bottom_y - corner;
    int16_t bottom_right_center_x = right_x - corner;
    int16_t bottom_right_center_y = bottom_y - corner;

    if (fill_color != -1)
    {
        // fill inner retangle first
        for (int i = x; i <= right_x; i++)
        {
            if (i < top_left_center_x || i > top_right_center_x)
                ssd1306_draw_stright_line(&gs_handle, i, top_left_center_y, i, bottom_left_center_y, fill_color);
            else
                ssd1306_draw_stright_line(&gs_handle, i, y, i, bottom_y, fill_color);
        }
        // fill four corner
        ssd1306_oled_draw_circle(top_left_center_x, top_left_center_y, corner, fill_color, fill_color);
        ssd1306_oled_draw_circle(top_right_center_x, top_right_center_y, corner, fill_color, fill_color);
        ssd1306_oled_draw_circle(bottom_left_center_x, bottom_left_center_y, corner, fill_color, fill_color);
        ssd1306_oled_draw_circle(bottom_right_center_x, bottom_right_center_y, corner, fill_color, fill_color);
    }

    ssd1306_oled_draw_arc(top_left_center_x, top_left_center_y, corner, 180, 270, color);
    ssd1306_oled_draw_arc(top_right_center_x, top_right_center_y, corner, 270, 360, color);
    ssd1306_oled_draw_arc(bottom_right_center_x, bottom_right_center_y, corner, 0, 90, color);
    ssd1306_oled_draw_arc(bottom_left_center_x, bottom_left_center_y, corner, 90, 180, color);

    ssd1306_draw_stright_line(&gs_handle, x + corner, y, right_x - corner, y, color);
    ssd1306_draw_stright_line(&gs_handle, right_x, y + corner, right_x, bottom_y - corner, color);
    ssd1306_draw_stright_line(&gs_handle, x + corner, bottom_y, right_x - corner, bottom_y, color);
    ssd1306_draw_stright_line(&gs_handle, x, y + corner, x, bottom_y - corner, color);

    return 0;
}

static inline bool points_equal(Point a, Point b)
{
    return (a.x == b.x) && (a.y == b.y);
}

int ssd1306_oled_draw_polygon(const Point *vertices, uint8_t num_vertices, uint8_t color, int8_t fill_color)
{
    // there are issues with fill polygon
    if (num_vertices < 3)
        return -1;
    if (vertices == NULL)
        return -2;

    for (uint8_t i = 0; i < num_vertices; i++)
    {
        const Point *p1 = &vertices[i];
        const Point *p2 = &vertices[(i + 1) % num_vertices];

        if (points_equal(*p1, *p2))
            continue;

        ssd1306_oled_draw_line(p1->x, p1->y, p2->x, p2->y, color);
    }

    if (!fill_color)
        return 0;

    for (int16_t y = 0; y <= SSD1306_HEIGHT; y++)
    {
        bool inside = false;
        bool last_edge = false;

        for (int16_t x = 0; x <= SSD1306_WIDTH; x++)
        {
            bool is_edge = ssd1306_oled_get_point(x, y) == color;
            if (is_edge)
            {

                if (y > 0 && ssd1306_oled_get_point(x, y - 1) != color)
                {
                    inside = !inside;
                }
                last_edge = true;
            }
            else
            {

                if (last_edge)
                {
                    inside = !inside;
                    last_edge = false;
                }
            }
            if (inside && !is_edge)
            {
                ssd1306_draw_point(&gs_handle, x, y, color);
            }
        }
    }

    return 0;
}

// The function has been verified.
int ssd1306_oled_draw_circle(int16_t center_x, int16_t center_y, uint8_t radius, uint8_t color, int8_t fill_color)
{
    if (radius == 0)
    {
        return ssd1306_draw_point(&gs_handle, center_x, center_y, color);
    }

    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 3 - 2 * radius;

    // fill inside
    if (fill_color != -1)
    {
        const uint8_t fill = (fill_color != 0) ? 1 : 0;
        const int16_t r = radius;

        for (int16_t dy = -r; dy <= r; ++dy)
        {
            const int16_t dx = (int16_t)sqrt(r * r - dy * dy);
            const int16_t x_start = center_x - dx;
            const int16_t x_end = center_x + dx;
            const int16_t y_current = center_y + dy;

            ssd1306_draw_stright_line(&gs_handle, x_start, y_current, x_end, y_current, fill);
        }
    }

    // draw border
    while (x <= y)
    {
        const int16_t points[8][2] = {
            {x, y},
            {y, x},
            {static_cast<int16_t>(-x), y},
            {static_cast<int16_t>(-y), x},
            {static_cast<int16_t>(-x), static_cast<int16_t>(-y)},
            {static_cast<int16_t>(-y), static_cast<int16_t>(-x)},
            {x, static_cast<int16_t>(-y)},
            {y, static_cast<int16_t>(-x)}};

        for (uint8_t i = 0; i < 8; ++i)
        {
            const int16_t px = center_x + points[i][0];
            const int16_t py = center_y + points[i][1];
            ssd1306_draw_point(&gs_handle, px, py, color);
        }

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    return 0;
}

// The function has been verified.
int ssd1306_oled_draw_arc(int16_t center_x, int16_t center_y, uint8_t radius, uint16_t start_angle, uint16_t end_angle, uint8_t color)
{
    if (radius == 0)
        return -1;

    if (start_angle > end_angle)
    {
        ssd1306_oled_draw_arc(20, 42, 22, start_angle, 360, 1);
        ssd1306_oled_draw_arc(20, 42, 22, 0, end_angle, 1);
        return 0;
    }

    start_angle %= 360;
    end_angle %= 360;

    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 3 - 2 * radius;

    while (x <= y)
    {
        const int16_t octant_points[8][2] = {
            {x, y},
            {y, x},
            {static_cast<int16_t>(-x), y},
            {static_cast<int16_t>(-y), x},
            {static_cast<int16_t>(-x), static_cast<int16_t>(-y)},
            {static_cast<int16_t>(-y), static_cast<int16_t>(-x)},
            {x, static_cast<int16_t>(-y)},
            {y, static_cast<int16_t>(-x)}};

        for (uint8_t octant = 0; octant < 8; ++octant)
        {
            const int16_t dx = octant_points[octant][0];
            const int16_t dy = octant_points[octant][1];
            const int16_t px = center_x + dx;
            const int16_t py = center_y + dy;

            double angle_deg = atan2(dy, dx) * 180.0 / M_PI;
            if (angle_deg < 0)
                angle_deg += 360.0;

            bool should_draw = false;
            if (start_angle <= end_angle)
            {
                should_draw = (angle_deg >= start_angle) && (angle_deg <= end_angle);
            }
            else
            {
                should_draw = (angle_deg >= start_angle) || (angle_deg <= end_angle);
            }

            if (should_draw)
            {
                ssd1306_draw_point(&gs_handle, px, py, color);
            }
        }

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    return 0;
}

// The function has been verified.
int ssd1306_oled_draw_sector(int16_t center_x, int16_t center_y, uint8_t radius, uint16_t start_angle, uint16_t end_angle, int8_t color, int8_t fill_color)
{
    if (radius == 0)
        return -1;

    if (start_angle > end_angle)
    {
        ssd1306_oled_draw_sector(21, 41, 20, start_angle, 360, 1, 1);
        ssd1306_oled_draw_sector(21, 41, 20, 0, end_angle, 1, 1);
        return 0;
    }

    start_angle %= 360;
    end_angle %= 360;

    // fill inside
    if (fill_color != -1)
    {
        const uint8_t fill = (fill_color != 0) ? 1 : 0;
        const int16_t r_squared = radius * radius;

        for (int16_t dy = -radius; dy <= radius; ++dy)
        {
            for (int16_t dx = -radius; dx <= radius; ++dx)
            {
                if (dx * dx + dy * dy > r_squared)
                    continue;

                double angle = atan2(dy, dx) * 180.0 / M_PI;
                if (angle < 0)
                    angle += 360.0;

                bool in_arc = (start_angle <= end_angle) ? (angle >= start_angle && angle <= end_angle) : (angle >= start_angle || angle <= end_angle);

                if (in_arc)
                {
                    ssd1306_draw_point(&gs_handle,
                                       center_x + dx, center_y + dy, fill);
                }
            }
        }
    }

    // draw boder
    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 3 - 2 * radius;

    while (x <= y)
    {
        const int16_t points[8][2] = {
            {x, y},
            {y, x},
            {static_cast<int16_t>(-x), y},
            {static_cast<int16_t>(-y), x},
            {static_cast<int16_t>(-x), static_cast<int16_t>(-y)},
            {static_cast<int16_t>(-y), static_cast<int16_t>(-x)},
            {x, static_cast<int16_t>(-y)},
            {y, static_cast<int16_t>(-x)}};

        for (uint8_t i = 0; i < 8; ++i)
        {
            const int16_t dx = points[i][0];
            const int16_t dy = points[i][1];

            double angle = atan2(dy, dx) * 180.0 / M_PI;
            if (angle < 0)
                angle += 360.0;

            bool in_arc = (start_angle <= end_angle) ? (angle >= start_angle && angle <= end_angle) : (angle >= start_angle || angle <= end_angle);

            if (in_arc)
            {
                ssd1306_draw_point(&gs_handle, center_x + dx, center_y + dy, color);
            }
        }

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    // draw Radius Line
    auto draw_radius = [&](uint16_t angle)
    {
        const double rad = angle * M_PI / 180.0;
        const int16_t end_x = center_x + radius * cos(rad);
        const int16_t end_y = center_y + radius * sin(rad);
        ssd1306_oled_draw_line(center_x, center_y, end_x, end_y, color);
    };

    draw_radius(start_angle);
    draw_radius(end_angle);

    return 0;
}

static void draw_ellipse_points(int16_t cx, int16_t cy, uint8_t x, uint8_t y, uint8_t color, int8_t fill_color)
{
    const int16_t points[4][2] = {
        {x, y},
        {static_cast<int16_t>(-x), y},
        {x, static_cast<int16_t>(-y)},
        {static_cast<int16_t>(-x), static_cast<int16_t>(-y)}};

    for (uint8_t i = 0; i < 4; i++)
    {
        int px = cx + points[i][0];
        int py = cy + points[i][1];

        if (px >= 0 && px <= SSD1306_WIDTH && py >= 0 && py <= SSD1306_HEIGHT)
        {
            ssd1306_draw_point(&gs_handle, px, py, color);

            if (fill_color && x != 0 && y != 0)
            {
                for (int fill_x = cx - x + 1; fill_x < cx + x; fill_x++)
                {
                    if (fill_x >= 0 && fill_x <= SSD1306_WIDTH)
                    {
                        ssd1306_draw_point(&gs_handle, fill_x, py, color);
                    }
                }
            }
        }
    }
}

int ssd1306_oled_draw_ellipse(int16_t center_x, int16_t center_y, uint16_t width, uint16_t height, uint8_t color, int8_t fill_color)
{
    if (width == 0 || height == 0)
        return -1;
    if (width > SSD1306_WIDTH || height > SSD1306_HEIGHT)
        return -1;

    uint8_t a = width / 2;
    uint8_t b = height / 2;
    uint16_t a2 = a * a;
    uint16_t b2 = b * b;

    uint8_t x = 0, y = b;
    int dx = 0, dy = a;
    int err = 0;

    while (dx * b2 <= dy * a2)
    {
        draw_ellipse_points(center_x, center_y, x, y, color, fill_color);

        int err_new = err + (2 * dx + 1) * b2;
        if (err_new >= 0)
        {
            y--;
            err += (2 * dx + 1) * b2 - 2 * dy * a2;
            dy -= a2;
        }
        else
        {
            err += (2 * dx + 1) * b2;
        }
        x++;
        dx += b2;
    }

    y = 0;
    x = a;
    dx = b2 * a;
    dy = 0;
    err = 0;

    while (dx >= dy)
    {
        draw_ellipse_points(center_x, center_y, x, y, color, fill_color);

        int err_new = err + (2 * dy + 1) * a2;
        if (err_new >= 0)
        {
            x--;
            err += (2 * dy + 1) * a2 - 2 * dx * b2;
            dx -= b2;
        }
        else
        {
            err += (2 * dy + 1) * a2;
        }
        y++;
        dy += a2;
    }

    return 0;
}

// The function has been verified.
int ssd1306_oled_draw_bitmap(int16_t x, int16_t y, uint8_t color, bool center, bool transparent, const OLEDIcon &icon)
{
    if (center)
    {
        x -= icon.width / 2;
        y -= icon.height / 2;
    }

    for (int row = 0; row < icon.height; row++)
    {
        for (int col = 0; col < icon.width; col++)
        {
            int16_t screen_x = x + col;
            int16_t screen_y = y + row;
            uint8_t back_color = color == 0 ? 1 : 0;
            if (icon.pixels[row][col])
                ssd1306_draw_point(&gs_handle, screen_x, screen_y, color);
            else if (!transparent)
                ssd1306_draw_point(&gs_handle, screen_x, screen_y, back_color);
        }
    }
    return 0;
}

int ssd1306_oled_enable_zoom_in(void)
{
    return ssd1306_set_zoom_in(&gs_handle, SSD1306_ZOOM_IN_ENABLE);
}

int ssd1306_oled_disable_zoom_in(void)
{
    return ssd1306_set_zoom_in(&gs_handle, SSD1306_ZOOM_IN_DISABLE);
}

int ssd1306_oled_fade_blinking(ssd1306_fade_blinking_mode_t mode, uint8_t frames)
{
    return ssd1306_set_fade_blinking_mode(&gs_handle, mode, frames);
}

int ssd1306_oled_deactivate_scroll(void)
{
    return ssd1306_deactivate_scroll(&gs_handle);
}

// Do not push data to the screen when the scroll is activated.
int ssd1306_oled_vertical_left_horizontal_scroll(uint8_t start_page_addr, uint8_t end_page_addr, uint8_t rows, ssd1306_scroll_frame_t frames)
{
    if (ssd1306_deactivate_scroll(&gs_handle) != 0)
    {
        return -1;
    }

    if (ssd1306_set_vertical_left_horizontal_scroll(&gs_handle, start_page_addr, end_page_addr, rows, frames) != 0)
    {
        return -1;
    }

    if (ssd1306_activate_scroll(&gs_handle) != 0)
    {
        return -1;
    }

    return 0;
}
// Do not push data to the screen when the scroll is activated.
int ssd1306_oled_vertical_right_horizontal_scroll(uint8_t start_page_addr, uint8_t end_page_addr, uint8_t rows, ssd1306_scroll_frame_t frames)
{
    if (ssd1306_deactivate_scroll(&gs_handle) != 0)
    {
        return -1;
    }

    if (ssd1306_set_vertical_right_horizontal_scroll(&gs_handle, start_page_addr, end_page_addr, rows, frames) != 0)
    {
        return -1;
    }

    if (ssd1306_activate_scroll(&gs_handle) != 0)
    {
        return -1;
    }

    return 0;
}
