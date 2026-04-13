#include <stdio.h>
#include <math.h>

#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "esp_io_expander_tca9554.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "esp_axp2101_port.h"
#include "esp_3inch5_lcd_port.h"

#define EXAMPLE_PIN_I2C_SDA GPIO_NUM_8
#define EXAMPLE_PIN_I2C_SCL GPIO_NUM_7

#define EXAMPLE_DISPLAY_ROTATION 90

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES 480
#define EXAMPLE_LCD_V_RES 320
#else
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#endif

#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8)
#define I2C_PORT_NUM 0

#define MAX_RPM_DISPLAY 2750

#define BOOST_MIN_PSI   (-15)
#define BOOST_MAX_PSI   60

#define EGT_MIN_F       0
#define EGT_WARN_F      1100
#define EGT_DANGER_F    1250
#define EGT_MAX_F       1400

#define UI_UPDATE_MS    100
#define SIM_UPDATE_MS   80

#define BOOST_HISTORY_LEN 30

#define MAX31855_CLK GPIO_NUM_11
#define MAX31855_DO  GPIO_NUM_9
#define MAX31855_CS  GPIO_NUM_17

static const char *TAG = "engine_dash";

i2c_master_bus_handle_t i2c_bus_handle;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;

lv_display_t *lvgl_disp = NULL;
lv_indev_t *lvgl_touch_indev = NULL;

static int32_t rpm_value = 850;
static int32_t boost_value = -10;
static int32_t egt_value = 450;

static int rpm_step = 60;
static int boost_step = 2;

static int32_t boost_peak = BOOST_MIN_PSI;
static int32_t boost_min_seen = BOOST_MAX_PSI;
static int32_t egt_peak = EGT_MIN_F;

static int32_t boost_history[BOOST_HISTORY_LEN] = {0};
static uint32_t boost_history_index = 0;

typedef struct
{
    lv_obj_t *card;
    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *unit_label;
    lv_obj_t *peak_label;
    lv_obj_t *min_label;

    int32_t min_value;
    int32_t max_value;
    int32_t warn_start;
    int32_t danger_start;
    bool use_threshold_colors;
} gauge_ui_t;

static lv_obj_t *tileview = NULL;
static lv_obj_t *tile_main = NULL;
static lv_obj_t *tile_rpm = NULL;
static lv_obj_t *tile_boost = NULL;
static lv_obj_t *tile_egt = NULL;

static gauge_ui_t main_rpm;
static gauge_ui_t main_boost;
static gauge_ui_t main_egt;

static gauge_ui_t rpm_full;
static gauge_ui_t boost_full;
static gauge_ui_t egt_full;

static lv_obj_t *boost_chart = NULL;
static lv_chart_series_t *boost_series = NULL;

void i2c_bus_init(void);
void io_expander_init(void);
void lv_port_init(void);
void max31855_init(void);
bool max31855_read_fahrenheit(float *temp_f);

void dashboard_ui_init(void);
void create_main_dashboard(lv_obj_t *parent);
void create_rpm_screen(lv_obj_t *parent);
void create_boost_screen(lv_obj_t *parent);
void create_egt_screen(lv_obj_t *parent);

void sensor_sim_task(void *arg);
void ui_update_task(void *arg);

static void style_bg(lv_obj_t *obj);
static void style_card(lv_obj_t *obj);
static void create_header(lv_obj_t *parent, const char *title, const char *subtitle);
static lv_obj_t *create_separator(lv_obj_t *parent, int width);

static void digital_card_init(gauge_ui_t *g,
                              lv_obj_t *parent,
                              const char *title,
                              const char *unit,
                              int32_t min_value,
                              int32_t max_value,
                              int32_t warn_start,
                              int32_t danger_start,
                              bool use_threshold_colors,
                              int width,
                              int height,
                              int value_offset_y);

static void digital_card_set_value(gauge_ui_t *g, int32_t value, const char *value_fmt);
static void set_peak_text(gauge_ui_t *g, const char *label, int32_t value);
static void set_min_text(gauge_ui_t *g, const char *label, int32_t value);
static void clear_stat_text(gauge_ui_t *g);

static lv_color_t color_bg(void)      { return lv_color_hex(0x090B0F); }
static lv_color_t color_card(void)    { return lv_color_hex(0x12161C); }
static lv_color_t color_border(void)  { return lv_color_hex(0x232B36); }
static lv_color_t color_text(void)    { return lv_color_hex(0xF3F5F7); }
static lv_color_t color_muted(void)   { return lv_color_hex(0x8E99A8); }
static lv_color_t color_blue(void)    { return lv_color_hex(0x3BA7FF); }
static lv_color_t color_yellow(void)  { return lv_color_hex(0xF4C95D); }
static lv_color_t color_red(void)     { return lv_color_hex(0xFF5D5D); }

static void style_bg(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, color_bg(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
}

static void style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, color_card(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, color_border(), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_pad_all(obj, 10, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static void create_header(lv_obj_t *parent, const char *title, const char *subtitle)
{
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, color_text(), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 18, 12);

    lv_obj_t *sub_label = lv_label_create(parent);
    lv_label_set_text(sub_label, subtitle);
    lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sub_label, color_muted(), 0);
    lv_obj_align(sub_label, LV_ALIGN_TOP_RIGHT, -18, 12);
}

static lv_obj_t *create_separator(lv_obj_t *parent, int width)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, width, 1);
    lv_obj_set_style_bg_color(line, color_border(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    return line;
}

static void digital_card_init(gauge_ui_t *g,
                              lv_obj_t *parent,
                              const char *title,
                              const char *unit,
                              int32_t min_value,
                              int32_t max_value,
                              int32_t warn_start,
                              int32_t danger_start,
                              bool use_threshold_colors,
                              int width,
                              int height,
                              int value_offset_y)
{
    g->min_value = min_value;
    g->max_value = max_value;
    g->warn_start = warn_start;
    g->danger_start = danger_start;
    g->use_threshold_colors = use_threshold_colors;

    g->card = lv_obj_create(parent);
    lv_obj_set_size(g->card, width, height);
    style_card(g->card);

    g->title_label = lv_label_create(g->card);
    lv_label_set_text(g->title_label, title);
    lv_obj_set_style_text_font(g->title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g->title_label, color_muted(), 0);
    lv_obj_align(g->title_label, LV_ALIGN_TOP_MID, 0, 14);

    g->value_label = lv_label_create(g->card);
    lv_label_set_text(g->value_label, "0");
    lv_obj_set_style_text_font(g->value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_letter_space(g->value_label, 2, 0);
    lv_obj_set_style_text_color(g->value_label, color_text(), 0);
    lv_obj_align(g->value_label, LV_ALIGN_CENTER, 0, value_offset_y);

    g->unit_label = lv_label_create(g->card);
    lv_label_set_text(g->unit_label, unit);
    lv_obj_set_style_text_font(g->unit_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g->unit_label, color_muted(), 0);
    lv_obj_align(g->unit_label, LV_ALIGN_CENTER, 0, value_offset_y + 34);

    g->peak_label = lv_label_create(g->card);
    lv_label_set_text(g->peak_label, "");
    lv_obj_set_style_text_font(g->peak_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g->peak_label, color_muted(), 0);
    lv_obj_align(g->peak_label, LV_ALIGN_BOTTOM_LEFT, 16, -12);

    g->min_label = lv_label_create(g->card);
    lv_label_set_text(g->min_label, "");
    lv_obj_set_style_text_font(g->min_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g->min_label, color_muted(), 0);
    lv_obj_align(g->min_label, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
}

static void digital_card_set_value(gauge_ui_t *g, int32_t value, const char *value_fmt)
{
    if (value < g->min_value) value = g->min_value;
    if (value > g->max_value) value = g->max_value;

    char buf[32];
    snprintf(buf, sizeof(buf), value_fmt, (long)value);
    lv_label_set_text(g->value_label, buf);

    if (!g->use_threshold_colors)
    {
        lv_obj_set_style_text_color(g->value_label, color_text(), 0);
        return;
    }

    if (value >= g->danger_start)
    {
        lv_obj_set_style_text_color(g->value_label, color_red(), 0);
    }
    else if (value >= g->warn_start)
    {
        lv_obj_set_style_text_color(g->value_label, color_yellow(), 0);
    }
    else
    {
        lv_obj_set_style_text_color(g->value_label, color_text(), 0);
    }
}

static void set_peak_text(gauge_ui_t *g, const char *label, int32_t value)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%s %ld", label, (long)value);
    lv_label_set_text(g->peak_label, buf);
}

static void set_min_text(gauge_ui_t *g, const char *label, int32_t value)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%s %ld", label, (long)value);
    lv_label_set_text(g->min_label, buf);
}

static void clear_stat_text(gauge_ui_t *g)
{
    lv_label_set_text(g->peak_label, "");
    lv_label_set_text(g->min_label, "");
}

void max31855_init(void)
{
    gpio_config_t out_conf = {};
    out_conf.pin_bit_mask = (1ULL << MAX31855_CLK) | (1ULL << MAX31855_CS);
    out_conf.mode = GPIO_MODE_OUTPUT;
    out_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    out_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&out_conf));

    gpio_config_t in_conf = {};
    in_conf.pin_bit_mask = (1ULL << MAX31855_DO);
    in_conf.mode = GPIO_MODE_INPUT;
    in_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    in_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&in_conf));

    gpio_set_level(MAX31855_CS, 1);
    gpio_set_level(MAX31855_CLK, 0);
}

bool max31855_read_fahrenheit(float *temp_f)
{
    if (temp_f == NULL)
    {
        return false;
    }

    uint32_t raw = 0;

    gpio_set_level(MAX31855_CS, 0);
    esp_rom_delay_us(1);

    for (int i = 31; i >= 0; i--)
    {
        gpio_set_level(MAX31855_CLK, 1);
        esp_rom_delay_us(1);

        if (gpio_get_level(MAX31855_DO))
        {
            raw |= (1UL << i);
        }

        gpio_set_level(MAX31855_CLK, 0);
        esp_rom_delay_us(1);
    }

    gpio_set_level(MAX31855_CS, 1);

    // Fault bit
    if (raw & 0x00010000)
    {
        return false;
    }

    // Bits 31:18 = signed 14-bit thermocouple temperature, 0.25C per bit
    int16_t tc_data = (raw >> 18) & 0x3FFF;

    if (tc_data & 0x2000)
    {
        tc_data |= 0xC000;
    }

    float temp_c = tc_data * 0.25f;
    *temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;

    return true;
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    for (int i = 0; i < BOOST_HISTORY_LEN; i++)
    {
        boost_history[i] = boost_value;
    }

    boost_peak = boost_value;
    boost_min_seen = boost_value;
    egt_peak = egt_value;

    i2c_bus_init();
    io_expander_init();
    max31855_init();

    esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
    esp_3inch5_touch_port_init(
        &touch_handle,
        i2c_bus_handle,
        EXAMPLE_LCD_H_RES,
        EXAMPLE_LCD_V_RES,
        EXAMPLE_DISPLAY_ROTATION
    );

    esp_axp2101_port_init(i2c_bus_handle);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_3inch5_brightness_port_init();
    esp_3inch5_brightness_port_set(80);

    lv_port_init();

    if (lvgl_port_lock(0))
    {
        dashboard_ui_init();
        lvgl_port_unlock();
    }

    xTaskCreate(sensor_sim_task, "sensor_sim_task", 4096, NULL, 5, NULL);
    xTaskCreate(ui_update_task, "ui_update_task", 4096, NULL, 5, NULL);
}

void i2c_bus_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {};
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = (i2c_port_num_t)I2C_PORT_NUM;
    i2c_mst_config.scl_io_num = EXAMPLE_PIN_I2C_SCL;
    i2c_mst_config.sda_io_num = EXAMPLE_PIN_I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = 1;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
}

void io_expander_init(void)
{
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(
        i2c_bus_handle,
        ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
        &expander_handle));

    ESP_ERROR_CHECK(esp_io_expander_set_dir(
        expander_handle,
        IO_EXPANDER_PIN_NUM_1,
        IO_EXPANDER_OUTPUT));

    ESP_ERROR_CHECK(esp_io_expander_set_level(
        expander_handle,
        IO_EXPANDER_PIN_NUM_1,
        0));

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(esp_io_expander_set_level(
        expander_handle,
        IO_EXPANDER_PIN_NUM_1,
        1));

    vTaskDelay(pdMS_TO_TICKS(100));
}

void lv_port_init(void)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");

    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .control_handle = NULL,
        .buffer_size = LCD_BUFFER_SIZE,
        .double_buffer = true,
        .trans_size = 0,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
        .flags = {
            .buff_dma = 0,
            .buff_spiram = 1,
            .sw_rotate = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

#if EXAMPLE_DISPLAY_ROTATION == 90
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 180
    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 270
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 0;
#endif

    lvgl_disp = lvgl_port_add_disp(&display_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };

    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
}

void dashboard_ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    style_bg(scr);

    tileview = lv_tileview_create(scr);
    lv_obj_set_size(tileview, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_obj_align(tileview, LV_ALIGN_CENTER, 0, 0);
    style_bg(tileview);

    tile_main  = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    tile_rpm   = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
    tile_boost = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);
    tile_egt   = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR);

    style_bg(tile_main);
    style_bg(tile_rpm);
    style_bg(tile_boost);
    style_bg(tile_egt);

    create_main_dashboard(tile_main);
    create_rpm_screen(tile_rpm);
    create_boost_screen(tile_boost);
    create_egt_screen(tile_egt);
}

void create_main_dashboard(lv_obj_t *parent)
{
    create_header(parent, "BETSY'S BOX", "MAIN");

    lv_obj_t *line = create_separator(parent, 444);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 46);

    digital_card_init(&main_rpm, parent, "RPM", "RPM",
                      0, MAX_RPM_DISPLAY, 0, 0, false,
                      140, 190, -12);
    lv_obj_align(main_rpm.card, LV_ALIGN_LEFT_MID, 14, 18);

    digital_card_init(&main_boost, parent, "BOOST", "PSI",
                      BOOST_MIN_PSI, BOOST_MAX_PSI, 0, 0, false,
                      140, 190, -12);
    lv_obj_align(main_boost.card, LV_ALIGN_CENTER, 0, 18);

    digital_card_init(&main_egt, parent, "EGT", "F",
                      EGT_MIN_F, EGT_MAX_F, EGT_WARN_F, EGT_DANGER_F, true,
                      140, 190, -12);
    lv_obj_align(main_egt.card, LV_ALIGN_RIGHT_MID, -14, 18);

    clear_stat_text(&main_rpm);
    clear_stat_text(&main_boost);
    clear_stat_text(&main_egt);
}

void create_rpm_screen(lv_obj_t *parent)
{
    create_header(parent, "BETSY'S BOX", "RPM");

    lv_obj_t *line = create_separator(parent, 444);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 46);

    digital_card_init(&rpm_full, parent, "RPM", "RPM",
                      0, MAX_RPM_DISPLAY, 0, 0, false,
                      420, 220, -8);
    lv_obj_align(rpm_full.card, LV_ALIGN_CENTER, 0, 18);

    lv_obj_set_style_text_letter_space(rpm_full.value_label, 4, 0);

    clear_stat_text(&rpm_full);
}

void create_boost_screen(lv_obj_t *parent)
{
    create_header(parent, "BETSY'S BOX", "BOOST");

    lv_obj_t *line = create_separator(parent, 444);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 46);

    digital_card_init(&boost_full, parent, "BOOST", "PSI",
                      BOOST_MIN_PSI, BOOST_MAX_PSI, 0, 0, false,
                      420, 150, -4);
    lv_obj_align(boost_full.card, LV_ALIGN_TOP_MID, 0, 60);

    boost_chart = lv_chart_create(parent);
    lv_obj_set_size(boost_chart, 420, 90);
    lv_obj_align(boost_chart, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_set_style_bg_color(boost_chart, color_card(), 0);
    lv_obj_set_style_bg_opa(boost_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(boost_chart, color_border(), 0);
    lv_obj_set_style_border_width(boost_chart, 1, 0);
    lv_obj_set_style_radius(boost_chart, 16, 0);
    lv_obj_set_style_pad_all(boost_chart, 8, 0);

    lv_chart_set_type(boost_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(boost_chart, BOOST_HISTORY_LEN);
    lv_chart_set_range(boost_chart, LV_CHART_AXIS_PRIMARY_Y, BOOST_MIN_PSI, BOOST_MAX_PSI);

    boost_series = lv_chart_add_series(boost_chart, color_blue(), LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_set_style_line_width(boost_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(boost_chart, 3, 0);
    lv_obj_set_style_pad_column(boost_chart, 2, 0);
}

void create_egt_screen(lv_obj_t *parent)
{
    create_header(parent, "BETSY'S BOX", "EGT");

    lv_obj_t *line = create_separator(parent, 444);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 46);

    digital_card_init(&egt_full, parent, "EGT", "F",
                      EGT_MIN_F, EGT_MAX_F, EGT_WARN_F, EGT_DANGER_F, true,
                      420, 220, -8);
    lv_obj_align(egt_full.card, LV_ALIGN_CENTER, 0, 18);

    lv_label_set_text(egt_full.min_label, "");
}

void sensor_sim_task(void *arg)
{
    while (1)
    {
        rpm_value += rpm_step;
        if (rpm_value >= 2750)
        {
            rpm_value = 2750;
            rpm_step = -70;
        }
        else if (rpm_value <= 750)
        {
            rpm_value = 750;
            rpm_step = 60;
        }

        boost_value += boost_step;
        if (boost_value >= 54)
        {
            boost_value = 54;
            boost_step = -2;
        }
        else if (boost_value <= -12)
        {
            boost_value = -12;
            boost_step = 2;
        }

        float egt_f = 0.0f;
        if (max31855_read_fahrenheit(&egt_f))
        {
            if (egt_f < EGT_MIN_F) egt_f = EGT_MIN_F;
            if (egt_f > EGT_MAX_F) egt_f = EGT_MAX_F;
            egt_value = (int32_t)(egt_f + 0.5f);
        }

        if (boost_value > boost_peak)
        {
            boost_peak = boost_value;
        }

        if (boost_value < boost_min_seen)
        {
            boost_min_seen = boost_value;
        }

        if (egt_value > egt_peak)
        {
            egt_peak = egt_value;
        }

        boost_history[boost_history_index] = boost_value;
        boost_history_index = (boost_history_index + 1) % BOOST_HISTORY_LEN;

        vTaskDelay(pdMS_TO_TICKS(SIM_UPDATE_MS));
    }
}

void ui_update_task(void *arg)
{
    while (1)
    {
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            digital_card_set_value(&main_rpm, rpm_value, "%ld");
            digital_card_set_value(&main_boost, boost_value, "%ld");
            digital_card_set_value(&main_egt, egt_value, "%ld");

            digital_card_set_value(&rpm_full, rpm_value, "%ld");

            digital_card_set_value(&boost_full, boost_value, "%ld");
            set_peak_text(&boost_full, "Peak", boost_peak);
            set_min_text(&boost_full, "Min", boost_min_seen);

            digital_card_set_value(&egt_full, egt_value, "%ld");
            set_peak_text(&egt_full, "Peak", egt_peak);

            if (boost_chart != NULL && boost_series != NULL)
            {
                for (int i = 0; i < BOOST_HISTORY_LEN; i++)
                {
                    uint32_t idx = (boost_history_index + i) % BOOST_HISTORY_LEN;
                    boost_series->y_points[i] = boost_history[idx];
                }
                lv_chart_refresh(boost_chart);
            }

            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(UI_UPDATE_MS));
    }
}