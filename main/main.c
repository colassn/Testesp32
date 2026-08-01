#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"

#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#define LCD_BACKLIGHT   GPIO_NUM_2
#define STRIP_HEIGHT    32

static const char *TAG = "RECOVERY";
static esp_lcd_panel_handle_t panel = NULL;
static uint16_t *strip = NULL;
static int strip_height = STRIP_HEIGHT;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

static void backlight_set(bool on)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LCD_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(LCD_BACKLIGHT, on ? 1 : 0);
}

static esp_err_t lcd_init(void)
{
    const esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = 14000000,
            .h_res = LCD_WIDTH,
            .v_res = LCD_HEIGHT,
            .hsync_pulse_width = 7,
            .hsync_back_porch = 40,
            .hsync_front_porch = 40,
            .vsync_pulse_width = 7,
            .vsync_back_porch = 10,
            .vsync_front_porch = 10,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 20 * LCD_WIDTH,
        .dma_burst_size = 64,
        .hsync_gpio_num = GPIO_NUM_39,
        .vsync_gpio_num = GPIO_NUM_41,
        .de_gpio_num = GPIO_NUM_40,
        .pclk_gpio_num = GPIO_NUM_42,
        .data_gpio_nums = {
            GPIO_NUM_8,  GPIO_NUM_3,  GPIO_NUM_46, GPIO_NUM_9,  GPIO_NUM_1,
            GPIO_NUM_5,  GPIO_NUM_6,  GPIO_NUM_7,  GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_4,
            GPIO_NUM_45, GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_14,
        },
        .disp_gpio_num = GPIO_NUM_NC,
        .flags = {
            .fb_in_psram = true,
        },
    };

    esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) return err;
    return esp_lcd_panel_init(panel);
}

static esp_err_t draw_solid(uint16_t color)
{
    for (int i = 0; i < LCD_WIDTH * strip_height; ++i) strip[i] = color;
    for (int y = 0; y < LCD_HEIGHT; y += strip_height) {
        int h = strip_height;
        if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
        esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y + h, strip);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static esp_err_t draw_bars(void)
{
    const uint16_t colors[] = {
        0xF800, 0xFFE0, 0x07E0, 0x07FF,
        0x001F, 0xF81F, 0xFFFF, 0x0000,
    };
    const int bar_w = LCD_WIDTH / 8;

    for (int y = 0; y < LCD_HEIGHT; y += strip_height) {
        int h = strip_height;
        if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
        for (int yy = 0; yy < h; ++yy) {
            for (int x = 0; x < LCD_WIDTH; ++x) {
                strip[yy * LCD_WIDTH + x] = colors[x / bar_w];
            }
        }
        esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y + h, strip);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static void fatal_blink(esp_err_t err)
{
    ESP_LOGE(TAG, "LCD init failed: %s (0x%x)", esp_err_to_name(err), (unsigned)err);
    while (true) {
        backlight_set(true);
        vTaskDelay(pdMS_TO_TICKS(250));
        backlight_set(false);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void app_main(void)
{
    backlight_set(false);
    ESP_LOGI(TAG, "ESP32-8048S050C recovery demo boot");
    ESP_LOGI(TAG, "Reset reason: %d", (int)esp_reset_reason());
    ESP_LOGI(TAG, "PSRAM initialized: %s", esp_psram_is_initialized() ? "yes" : "no");
    ESP_LOGI(TAG, "Free internal RAM: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    esp_err_t err = lcd_init();
    if (err != ESP_OK) fatal_blink(err);

    strip = heap_caps_malloc(LCD_WIDTH * strip_height * sizeof(uint16_t),
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!strip) {
        strip_height = 8;
        strip = heap_caps_malloc(LCD_WIDTH * strip_height * sizeof(uint16_t),
                                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!strip) {
        ESP_LOGE(TAG, "Unable to allocate LCD strip buffer");
        fatal_blink(ESP_ERR_NO_MEM);
    }

    backlight_set(true);
    ESP_LOGI(TAG, "LCD initialized; starting colour test");

    draw_solid(rgb565(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(800));
    draw_solid(rgb565(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(800));
    draw_solid(rgb565(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(800));
    draw_solid(rgb565(255, 255, 255));
    vTaskDelay(pdMS_TO_TICKS(800));
    draw_bars();

    unsigned heartbeat = 0;
    while (true) {
        ESP_LOGI(TAG, "Heartbeat %u: LCD task alive", heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
