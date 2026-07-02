#include "managers/gps_manager.h"
#include "core/callbacks.h"
#include "driver/periph_ctrl.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "managers/settings_manager.h"
<<<<<<< HEAD
=======
#include "managers/ghostchi_manager.h"
#include "managers/ghostscript_runtime.h"
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/uart_periph.h"
#include "sys/time.h"
#include "vendor/GPS/MicroNMEA.h"
#include "vendor/GPS/gps_logger.h"
#include <managers/views/terminal_screen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *GPS_TAG = "GPS";
static bool has_valid_cached_date = false;
static bool gps_connection_logged = false;
static TaskHandle_t gps_check_task_handle = NULL;
static bool gps_timeout_detected = false;
static void check_gps_connection_task(void *pvParameters);
<<<<<<< HEAD
=======
static void gps_soft_watchdog_task(void *pvParameters);
static void gps_soft_try_release_rgb_rmt(void);
static void gps_soft_try_reacquire_rgb_rmt(void);
static void gps_soft_prepare_rx_pin(void);

#define GPS_SOFT_WATCHDOG_POLL_MS 2000
#define GPS_SOFT_WATCHDOG_STALL_MS 12000
#define GPS_SOFT_WATCHDOG_RESTART_COOLDOWN_MS 30000
#define GPS_STALE_UPDATE_TIMEOUT_MS 3000

static const uint32_t gps_auto_baud_rates[] = {9600, 38400, 115200, 57600, 19200, 4800};

static int gps_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool gps_line_has_valid_checksum(const char *line, size_t len) {
    if (!line || len < 7 || line[0] != '$') {
        return false;
    }

    const char *asterisk = NULL;
    for (size_t i = 1; i < len; i++) {
        if (line[i] == '*') {
            asterisk = &line[i];
            break;
        }
        if (line[i] == '\r' || line[i] == '\n') {
            return false;
        }
    }

    if (!asterisk || (size_t)(asterisk - line + 2) >= len) {
        return false;
    }

    int hi = gps_hex_digit(asterisk[1]);
    int lo = gps_hex_digit(asterisk[2]);
    if (hi < 0 || lo < 0) {
        return false;
    }

    uint8_t crc = 0;
    for (const char *p = line + 1; p < asterisk; p++) {
        crc ^= (uint8_t)(*p);
    }

    return crc == (uint8_t)((hi << 4) | lo);
}

static bool gps_line_is_supported_nav_sentence(const char *line) {
    if (!line || line[0] != '$') {
        return false;
    }

    const char *asterisk = strchr(line, '*');
    size_t header_len = asterisk ? (size_t)(asterisk - line) : strlen(line);
    if (header_len < 6) {
        return false;
    }

    const char *type = line + 3;
    return strncmp(type, "GGA", 3) == 0 || strncmp(type, "RMC", 3) == 0 ||
           strncmp(type, "GLL", 3) == 0 || strncmp(type, "GSA", 3) == 0 ||
           strncmp(type, "GSV", 3) == 0 || strncmp(type, "VTG", 3) == 0;
}

static bool gps_probe_baud_once(uart_port_t uart_port, gpio_num_t rx_pin, uint32_t baud_rate) {
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_share_ensure_installed(uart_port, 1024, 0, 16) != ESP_OK ||
        uart_share_acquire(uart_port, UART_SHARE_OWNER_GPS, pdMS_TO_TICKS(1000)) != ESP_OK) {
        return false;
    }

    bool detected = false;
    if (uart_param_config(uart_port, &uart_config) == ESP_OK &&
        uart_set_pin(uart_port, UART_PIN_NO_CHANGE, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK) {
        uart_flush_input(uart_port);

        char line[128] = {0};
        size_t line_len = 0;
        bool in_sentence = false;
        uint8_t buf[96];
        TickType_t start = xTaskGetTickCount();
        const TickType_t timeout = pdMS_TO_TICKS(900);

        while ((xTaskGetTickCount() - start) < timeout && !detected) {
            int read_len = uart_read_bytes(uart_port, buf, sizeof(buf), pdMS_TO_TICKS(80));
            for (int i = 0; i < read_len && !detected; i++) {
                char c = (char)buf[i];
                if (c == '$') {
                    in_sentence = true;
                    line_len = 0;
                    line[line_len++] = c;
                    continue;
                }

                if (!in_sentence) {
                    continue;
                }

                if (line_len >= sizeof(line) - 1) {
                    in_sentence = false;
                    line_len = 0;
                    continue;
                }

                line[line_len++] = c;
                line[line_len] = '\0';

                if (c == '\n' || c == '\r') {
                    detected = gps_line_is_supported_nav_sentence(line) &&
                               gps_line_has_valid_checksum(line, line_len);
                    in_sentence = false;
                    line_len = 0;
                }
            }
        }
    }

    uart_flush_input(uart_port);
    (void)uart_share_release(uart_port, UART_SHARE_OWNER_GPS);
    return detected;
}

static bool gps_detect_baud(uart_port_t uart_port, gpio_num_t rx_pin, uint32_t *out_baud) {
    if (!out_baud) {
        return false;
    }

    for (size_t i = 0; i < sizeof(gps_auto_baud_rates) / sizeof(gps_auto_baud_rates[0]); i++) {
        uint32_t baud = gps_auto_baud_rates[i];
        glog("GPS auto baud: probing %lu...\n", (unsigned long)baud);
        if (gps_probe_baud_once(uart_port, rx_pin, baud)) {
            *out_baud = baud;
            glog("GPS auto baud: detected %lu.\n", (unsigned long)baud);
            return true;
        }
    }

    return false;
}

static void gps_soft_acquire_pm_lock(void) {
#ifdef CONFIG_PM_ENABLE
    if (gps_soft_pm_lock == NULL) {
        esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "gps_soft", &gps_soft_pm_lock);
        if (err != ESP_OK) {
            ESP_LOGW(GPS_TAG, "Failed to create GPS PM lock: %s", esp_err_to_name(err));
            return;
        }
    }
    esp_err_t err = esp_pm_lock_acquire(gps_soft_pm_lock);
    if (err != ESP_OK) {
        ESP_LOGW(GPS_TAG, "Failed to acquire GPS PM lock: %s", esp_err_to_name(err));
    }
#endif
}

static void gps_soft_release_pm_lock(void) {
#ifdef CONFIG_PM_ENABLE
    if (gps_soft_pm_lock != NULL) {
        esp_err_t err = esp_pm_lock_release(gps_soft_pm_lock);
        if (err != ESP_OK) {
            ESP_LOGW(GPS_TAG, "Failed to release GPS PM lock: %s", esp_err_to_name(err));
        }
    }
#endif
}

static void gps_soft_prepare_rx_pin(void) {
    if (gps_soft_rx_pin == GPIO_NUM_NC) {
        return;
    }

    gpio_reset_pin(gps_soft_rx_pin);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_direction(gps_soft_rx_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gps_soft_rx_pin, GPIO_PULLUP_ONLY);
}

static esp_err_t gps_soft_start_parser(void) {
    if (gps_soft_rx_pin == GPIO_NUM_NC || gps_soft_baud_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    gps_soft_prepare_rx_pin();

    nmea_hdl = minmea_soft_start(gps_soft_rx_pin, gps_soft_baud_rate);
    gps_soft_mode_active = (nmea_hdl != NULL);

    if (!nmea_hdl) {
        esp_err_t soft_err = minmea_soft_get_last_error();
        if (soft_err == ESP_ERR_NOT_FOUND) {
            gps_soft_try_release_rgb_rmt();
            gps_soft_released_rgb_rmt = true;
            nmea_hdl = minmea_soft_start(gps_soft_rx_pin, gps_soft_baud_rate);
            gps_soft_mode_active = (nmea_hdl != NULL);
        }
    }

    if (!nmea_hdl) {
        return minmea_soft_get_last_error();
    }

    gps_last_update_tick = 0;
    gps_has_seen_update = false;
    return ESP_OK;
}

static esp_err_t gps_soft_restart_parser(const char *reason, const minmea_soft_stats_t *stats) {
    if (gps_soft_rx_pin == GPIO_NUM_NC || gps_soft_baud_rate == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    glog("Soft GPS watchdog restart (%s): events=%lu edges=%lu edge_probe=%s qdrop=%lu rearm=%lu/%lu turn=%lu/%lu\n",
         reason ? reason : "unknown",
         (unsigned long)(stats ? stats->rx_events : 0),
         (unsigned long)(stats ? stats->raw_gpio_edges : 0),
         (stats && stats->edge_probe_ok) ? "ok" : "n/a",
         (unsigned long)(stats ? stats->rx_queue_drops : 0),
         (unsigned long)(stats ? stats->rx_rearm_failures : 0),
         (unsigned long)(stats ? stats->rx_rearm_recovers : 0),
         (unsigned long)(stats ? stats->rx_local_turnovers : 0),
         (unsigned long)(stats ? stats->rx_local_turnover_failures : 0));

    if (nmea_hdl) {
        minmea_soft_stop(nmea_hdl);
        nmea_hdl = NULL;
    }

    if (gps_soft_released_rgb_rmt) {
        gps_soft_try_reacquire_rgb_rmt();
        gps_soft_released_rgb_rmt = false;
    }

    esp_err_t err = gps_soft_start_parser();
    if (err == ESP_OK) {
        glog("Soft GPS watchdog restart OK (IO%d @ %lu).\n",
             (int)gps_soft_rx_pin,
             (unsigned long)gps_soft_baud_rate);
        return ESP_OK;
    }

    glog("Soft GPS watchdog restart failed: %s\n", esp_err_to_name(err));
    return err;
}

void gps_manager_set_peer_gps_preferred(bool enabled) {
    gps_peer_preferred = enabled;
    if (!enabled) {
        gps_peer_last_update_tick = 0;
        gps_peer_has_seen_update = false;
    }
}

bool gps_manager_is_peer_gps_preferred(void) {
    return gps_peer_preferred;
}

void gps_manager_clear_peer_fix(void) {
    taskENTER_CRITICAL(&gps_state_lock);
    gps_peer_last_update_tick = 0;
    gps_peer_has_seen_update = false;
    memset(&gps_peer_fix_snapshot, 0, sizeof(gps_peer_fix_snapshot));
    gps_peer_fix_snapshot.fix = GPS_FIX_INVALID;
    gps_peer_fix_snapshot.fix_mode = GPS_MODE_INVALID;
    taskEXIT_CRITICAL(&gps_state_lock);
}

void gps_manager_update_local_snapshot(const gps_t *fix) {
    if (!fix) {
        return;
    }

    taskENTER_CRITICAL(&gps_state_lock);
    gps_local_snapshot = *fix;
    taskEXIT_CRITICAL(&gps_state_lock);
    bool has_fix = fix->fix >= GPS_FIX_GPS && fix->fix_mode >= GPS_MODE_2D && fix->sats_in_use >= 3;
    char gps_payload[64];
    snprintf(gps_payload, sizeof(gps_payload), "%s|%.6f|%.6f|%.1f|%d",
        has_fix ? "yes" : "no", fix->latitude, fix->longitude, fix->altitude, fix->sats_in_use);
    ghostscript_emit_event(has_fix ? "gps_update" : "gps_fix", gps_payload);
}

void gps_manager_update_peer_fix(const gps_peer_fix_t *fix) {
    if (!fix) {
        return;
    }

    taskENTER_CRITICAL(&gps_state_lock);
    gps_peer_fix_snapshot.latitude = fix->latitude;
    gps_peer_fix_snapshot.longitude = fix->longitude;
    gps_peer_fix_snapshot.altitude = fix->altitude;
    gps_peer_fix_snapshot.speed = fix->speed;
    gps_peer_fix_snapshot.cog = fix->course;
    gps_peer_fix_snapshot.dop_h = fix->hdop;
    gps_peer_fix_snapshot.fix = fix->fix;
    gps_peer_fix_snapshot.fix_mode = fix->fix_mode;
    if (fix->date_valid) {
        gps_peer_fix_snapshot.date = fix->date;
        if (fix->date.year <= 99 && fix->date.month >= 1 && fix->date.month <= 12 &&
            fix->date.day >= 1 && fix->date.day <= 31) {
            cacheddate = fix->date;
            has_valid_cached_date = true;
        }
    }
    if (fix->time_valid) {
        gps_peer_fix_snapshot.tim = fix->tim;
    }
    gps_peer_fix_snapshot.sats_in_use = fix->sats_in_use;
    gps_peer_fix_snapshot.sats_in_view = fix->sats_in_view;
    gps_peer_fix_snapshot.valid = fix->valid;

    gps_peer_last_update_tick = xTaskGetTickCount();
    gps_peer_has_seen_update = true;
    taskEXIT_CRITICAL(&gps_state_lock);
    bool has_fix = fix->fix >= GPS_FIX_GPS && fix->fix_mode >= GPS_MODE_2D && fix->sats_in_use >= 3;
    char gps_payload[64];
    snprintf(gps_payload, sizeof(gps_payload), "%s|%.6f|%.6f|%.1f|%d",
        has_fix ? "yes" : "no", fix->latitude, fix->longitude, fix->altitude, fix->sats_in_use);
    ghostscript_emit_event(has_fix ? "gps_update" : "gps_fix", gps_payload);
}

bool gps_manager_get_local_gps_snapshot(gps_t *out_gps) {
    if (!out_gps) {
        return false;
    }

    if (!g_gpsManager.isinitilized || !gps_has_seen_update) {
        return false;
    }

    taskENTER_CRITICAL(&gps_state_lock);
    *out_gps = gps_local_snapshot;
    taskEXIT_CRITICAL(&gps_state_lock);
    return true;
}

bool gps_manager_get_active_gps_snapshot(gps_t *out_gps, bool *using_peer) {
    if (!out_gps) {
        return false;
    }

    if (gps_peer_preferred) {
        taskENTER_CRITICAL(&gps_state_lock);
        TickType_t last_tick = gps_peer_last_update_tick;
        if (last_tick != 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_tick) <= pdMS_TO_TICKS(GPS_STALE_UPDATE_TIMEOUT_MS)) {
                *out_gps = gps_peer_fix_snapshot;
                taskEXIT_CRITICAL(&gps_state_lock);
                if (using_peer) {
                    *using_peer = true;
                }
                return true;
            }
        }
        taskEXIT_CRITICAL(&gps_state_lock);
        if (using_peer) {
            *using_peer = true;
        }
        return false;
    }

    if (!gps_manager_get_local_gps_snapshot(out_gps)) {
        if (using_peer) {
            *using_peer = false;
        }
        return false;
    }

    if (using_peer) {
        *using_peer = false;
    }
    return true;
}

typedef struct {
    bool valid;
    gps_fix_t fix;
    gps_fix_mode_t fix_mode;
    uint8_t sats_in_use;
    uint8_t sats_in_view;
    float latitude;
    float longitude;
    float altitude;
    float dop_h;
    float dop_p;
    float dop_v;
    float speed;
    float cog;
    float variation;
    gps_date_t date;
    gps_time_t tim;
} gps_wd_lite_t;

static bool gps_manager_get_wd_lite(gps_wd_lite_t *out, bool *using_peer) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    if (gps_peer_preferred) {
        taskENTER_CRITICAL(&gps_state_lock);
        TickType_t last_tick = gps_peer_last_update_tick;
        if (last_tick != 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_tick) <= pdMS_TO_TICKS(GPS_STALE_UPDATE_TIMEOUT_MS)) {
                const gps_t *src = &gps_peer_fix_snapshot;
                out->valid = src->valid;
                out->fix = src->fix;
                out->fix_mode = src->fix_mode;
                out->sats_in_use = src->sats_in_use;
                out->sats_in_view = src->sats_in_view;
                out->latitude = src->latitude;
                out->longitude = src->longitude;
                out->altitude = src->altitude;
                out->dop_h = src->dop_h;
                out->dop_p = src->dop_p;
                out->dop_v = src->dop_v;
                out->speed = src->speed;
                out->cog = src->cog;
                out->variation = src->variation;
                out->date = src->date;
                out->tim = src->tim;
                taskEXIT_CRITICAL(&gps_state_lock);
                if (using_peer) *using_peer = true;
                return true;
            }
        }
        taskEXIT_CRITICAL(&gps_state_lock);
        if (using_peer) *using_peer = true;
        return false;
    }

    if (!g_gpsManager.isinitilized || !gps_has_seen_update) {
        if (using_peer) *using_peer = false;
        return false;
    }

    taskENTER_CRITICAL(&gps_state_lock);
    {
        const gps_t *src = &gps_local_snapshot;
        out->valid = src->valid;
        out->fix = src->fix;
        out->fix_mode = src->fix_mode;
        out->sats_in_use = src->sats_in_use;
        out->sats_in_view = src->sats_in_view;
        out->latitude = src->latitude;
        out->longitude = src->longitude;
        out->altitude = src->altitude;
        out->dop_h = src->dop_h;
        out->dop_p = src->dop_p;
        out->dop_v = src->dop_v;
        out->speed = src->speed;
        out->cog = src->cog;
        out->variation = src->variation;
        out->date = src->date;
        out->tim = src->tim;
    }
    taskEXIT_CRITICAL(&gps_state_lock);
    if (using_peer) *using_peer = false;
    return true;
}

static bool gps_should_preserve_dualcomm(void) {
#ifdef CONFIG_BUILD_CONFIG_TEMPLATE
    if (strcmp(CONFIG_BUILD_CONFIG_TEMPLATE, "somethingsomething") == 0 ||
        strcmp(CONFIG_BUILD_CONFIG_TEMPLATE, "somethingsomething2") == 0) {
        return true;
    }
#endif
    return false;
}

static bool gps_should_use_software_rx(void) {
#ifdef CONFIG_BUILD_CONFIG_TEMPLATE
    if (strcmp(CONFIG_BUILD_CONFIG_TEMPLATE, "somethingsomething") == 0) {
        return true;
    }
#endif
    return false;
}

static void gps_soft_try_release_rgb_rmt(void) {
#if defined(CONFIG_IDF_TARGET_ESP32C5)
    rgb_manager_rmt_release();
#endif
}

static void gps_soft_try_reacquire_rgb_rmt(void) {
#if defined(CONFIG_IDF_TARGET_ESP32C5)
    rgb_manager_rmt_reacquire();
#endif
}
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)

nmea_parser_handle_t nmea_hdl;

gps_date_t cacheddate = {0};

static bool is_valid_date(const gps_date_t *date) {
    if (!date)
        return false;

    // Check year (0-99 represents 2000-2099)
    if (!gps_is_valid_year(date->year))
        return false;

    // Check month (1-12)
    if (date->month < 1 || date->month > 12)
        return false;

    // Check day (1-31 depending on month)
    uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Adjust February for leap years
    uint16_t absolute_year = gps_get_absolute_year(date->year);
    if ((absolute_year % 4 == 0 && absolute_year % 100 != 0) || (absolute_year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    if (date->day < 1 || date->day > days_in_month[date->month - 1])
        return false;

    return true;
}

void gps_manager_init(GPSManager *manager) {
    // If there's an existing check task, delete it
    if (gps_check_task_handle != NULL) {
        vTaskDelete(gps_check_task_handle);
        gps_check_task_handle = NULL;
    }

    // Reset connection logged state
    gps_connection_logged = false;

    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
    uint8_t current_rx_pin = settings_get_gps_rx_pin(&G_Settings);

    if (current_rx_pin != 0) {
        printf("GPS RX: IO%d\n", current_rx_pin);
        TERMINAL_VIEW_ADD_TEXT("GPS RX: IO%d\n", current_rx_pin);

        // Only disable UART1 which we use for GPS
        periph_module_disable(PERIPH_UART1_MODULE);

        gpio_reset_pin(current_rx_pin);
        vTaskDelay(pdMS_TO_TICKS(10));

        periph_module_enable(PERIPH_UART1_MODULE);

        gpio_set_direction(current_rx_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(current_rx_pin, GPIO_FLOATING);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[current_rx_pin], UART_PIN_NO_CHANGE);

        config.uart.rx_pin = current_rx_pin;
        config.uart.uart_port = UART_NUM_1; // Explicitly set UART1 for GPS
    }

#ifdef CONFIG_IS_GHOST_BOARD
    config.uart.rx_pin = 2;
#endif

    nmea_hdl = nmea_parser_init(&config);
    nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
    manager->isinitilized = true;
    xTaskCreate(check_gps_connection_task, "gps_check", 2048, NULL, 1, &gps_check_task_handle);
}

static void check_gps_connection_task(void *pvParameters) {
    const TickType_t timeout = pdMS_TO_TICKS(10000); // 10 second timeout
    TickType_t start_time = xTaskGetTickCount();

    while (xTaskGetTickCount() - start_time < timeout) {
        if (!nmea_hdl) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        gps_t *gps = &((esp_gps_t *)nmea_hdl)->parent;

        if (!gps) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Check if we're receiving valid GPS data
        if (!gps_connection_logged &&
            (gps->tim.hour != 0 || gps->tim.minute != 0 || gps->tim.second != 0 ||
             gps->latitude != 0 || gps->longitude != 0)) {
            printf("GPS Module Connected\nReceiving Data\n");
            TERMINAL_VIEW_ADD_TEXT("GPS Module Connected\nReceiving Data\n");
            gps_connection_logged = true;
            gps_check_task_handle = NULL;
            vTaskDelete(NULL);
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // If we reach here, connection check timed out
    printf("GPS Module Connection Timeout\nCheck your connections\n");
    TERMINAL_VIEW_ADD_TEXT("GPS Module Connection Timeout\nCheck your connections\n");
    gps_timeout_detected = true;
    gps_check_task_handle = NULL;
    vTaskDelete(NULL);
}

void gps_manager_deinit(GPSManager *manager) {
    if (manager->isinitilized) {
        // If there's an existing check task, delete it
        if (gps_check_task_handle != NULL) {
            vTaskDelete(gps_check_task_handle);
            gps_check_task_handle = NULL;
        }

        nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
        nmea_parser_deinit(nmea_hdl);
        manager->isinitilized = false;
        gps_connection_logged = false;
    }
}

#define GPS_STATUS_MESSAGE "GPS: %s\nSats: %u/%u\nSpeed: %.1f km/h\nAccuracy: %s\n"
#define GPS_UPDATE_INTERVAL 4 // Show status every 4th update (25% chance)

#define MIN_SPEED_THRESHOLD 0.1   // Minimum 0.1 m/s (~0.36 km/h)
#define MAX_SPEED_THRESHOLD 340.0 // Maximum 340 m/s (~1224 km/h)

esp_err_t gps_manager_log_wardriving_data(wardriving_data_t *data) {
    if (!data || !nmea_hdl) {
        return ESP_ERR_INVALID_ARG;
    }
    gps_t *gps = &((esp_gps_t *)nmea_hdl)->parent;
    if (!data->ble_data.is_ble_device) {
        if (!gps->valid || strlen(data->ssid) <= 2) {
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        // For BLE entries, only check GPS validity
        if (!gps->valid || gps->fix < GPS_FIX_GPS || gps->fix_mode < GPS_MODE_2D ||
            gps->sats_in_use < 3 || gps->sats_in_use > GPS_MAX_SATELLITES_IN_USE) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Validate GPS data
    if (!is_valid_date(&gps->date)) {
        if (!has_valid_cached_date) {
            ESP_LOGW(GPS_TAG, "No valid GPS date available");
            return ESP_ERR_INVALID_STATE;
        }

        // Only log warning for good GPS fixes
        if (gps->valid && gps->fix >= GPS_FIX_GPS && gps->fix_mode >= GPS_MODE_2D &&
            gps->sats_in_use >= 3 && gps->sats_in_use <= GPS_MAX_SATELLITES_IN_USE &&
            rand() % 100 == 0) {
            ESP_LOGW(GPS_TAG,
                     "Invalid date despite good fix: %04d-%02d-%02d "
                     "(Fix: %d, Mode: %d, Sats: %d)",
                     gps_get_absolute_year(gps->date.year), gps->date.month, gps->date.day,
                     gps->fix, gps->fix_mode, gps->sats_in_use);
        }

        // Use cached date for validation
        ESP_LOGD(GPS_TAG, "Using cached date: %04d-%02d-%02d",
                 gps_get_absolute_year(cacheddate.year), cacheddate.month, cacheddate.day);
    } else if (!has_valid_cached_date) {
        // Valid date - update cache
        cacheddate = gps->date;
        has_valid_cached_date = true;
        ESP_LOGI(GPS_TAG, "Cached valid GPS date: %04d-%02d-%02d",
                 gps_get_absolute_year(cacheddate.year), cacheddate.month, cacheddate.day);
    }

    data->latitude = gps->latitude;
    data->longitude = gps->longitude;
    data->altitude = gps->altitude;
    data->accuracy = gps->dop_h * 5.0;

    // First, validate the current GPS date
    if (!is_valid_date(&gps->date)) {
        // Only show warning if we have a truly valid fix
        if (gps->valid && gps->fix >= GPS_FIX_GPS && gps->fix_mode >= GPS_MODE_2D &&
            gps->sats_in_use >= 3 &&
            gps->sats_in_use <= GPS_MAX_SATELLITES_IN_USE && // Should be ≤ 12
            rand() % 100 == 0) {
            printf("Warning: GPS date is out of range despite good fix: %04d-%02d-%02d "
                   "(Fix: %d, Mode: %d, Sats: %d)\n",
                   gps_get_absolute_year(gps->date.year), gps->date.month, gps->date.day, gps->fix,
                   gps->fix_mode, gps->sats_in_use);
        }
        return ESP_OK;
    }

    // Then, only if we don't have a cached date and the current date is valid,
    // cache it
    if (cacheddate.year <= 0) {
        cacheddate = gps->date;
    }

    if (gps->tim.hour > 23 || gps->tim.minute > 59 || gps->tim.second > 59) {
        if (rand() % 20 == 0) {
            printf("Warning: GPS time is invalid: %02d:%02d:%02d\n", gps->tim.hour, gps->tim.minute,
                   gps->tim.second);
        }
        return ESP_OK;
    }

    if (gps->latitude < -90.0 || gps->latitude > 90.0 || gps->longitude < -180.0 ||
        gps->longitude > 180.0) {
        if (rand() % 20 == 0) {
            printf("GPS Error: Invalid location detected (Lat: %f, Lon: %f)\n", gps->latitude,
                   gps->longitude);
        }
        return ESP_OK;
    }

    if (gps->speed < 0.0 || gps->speed > 340.0) {
        if (rand() % 20 == 0) {
            printf("Warning: GPS speed is out of range: %f m/s\n", gps->speed);
        }
        return ESP_OK;
    }

    if (gps->dop_h < 0.0 || gps->dop_p < 0.0 || gps->dop_v < 0.0 || gps->dop_h > 50.0 ||
        gps->dop_p > 50.0 || gps->dop_v > 50.0) {
        if (rand() % 20 == 0) {
            printf("Warning: GPS DOP values are out of range: HDOP: %f, PDOP: %f, "
                   "VDOP: %f\n",
                   gps->dop_h, gps->dop_p, gps->dop_v);
        }
        return ESP_OK;
    }

    esp_err_t ret = csv_write_data_to_buffer(data);
    if (ret != ESP_OK) {
        ESP_LOGE(GPS_TAG, "Failed to write wardriving data to CSV buffer");
        return ret;
    }

    // Update display periodically
    if (rand() % GPS_UPDATE_INTERVAL == 0) {
        // Determine GPS fix status
        const char *fix_status = (!gps->valid || gps->fix == GPS_FIX_INVALID) ? "No Fix"
                                 : (gps->fix_mode == GPS_MODE_2D)             ? "Basic"
                                 : (gps->fix_mode == GPS_MODE_3D)             ? "Locked"
                                                                              : "Unknown";

        // Validate satellite counts (clamp between 0 and max)
        uint8_t sats_in_use = (gps->sats_in_use > GPS_MAX_SATELLITES_IN_USE) ? 0 : gps->sats_in_use;

        // Determine accuracy based on HDOP
        const char *accuracy = (gps->dop_h < 0.0 || gps->dop_h > 50.0) ? "Invalid"
                               : (gps->dop_h <= 1.0)                   ? "Perfect"
                               : (gps->dop_h <= 2.0)                   ? "High"
                               : (gps->dop_h <= 5.0)                   ? "Good"
                               : (gps->dop_h <= 10.0)                  ? "Okay"
                                                                       : "Poor";

        // Convert speed from m/s to km/h for display with validation
        float speed_kmh = 0.0;
        if (gps->valid && gps->fix >= GPS_FIX_GPS) { // Only trust speed with a valid fix
            if (gps->speed >= MIN_SPEED_THRESHOLD && gps->speed <= MAX_SPEED_THRESHOLD) {
                speed_kmh = gps->speed * 3.6; // Convert m/s to km/h
            } else if (gps->speed < MIN_SPEED_THRESHOLD && gps->speed >= 0.0) {
                speed_kmh = 0.0; // Show as stopped if below threshold but not negative
            }
            // Speeds above MAX_SPEED_THRESHOLD remain at 0.0
        }

        // Add newline before status update for better readability
        printf("\n");
        printf(
            GPS_STATUS_MESSAGE, fix_status, data->gps_quality.satellites_used,
            GPS_MAX_SATELLITES_IN_USE,
            data->gps_quality.speed * 3.6, // Convert m/s to km/h
            get_gps_quality_string(data)); // Only keep the arguments that match the format string
        TERMINAL_VIEW_ADD_TEXT(GPS_STATUS_MESSAGE, fix_status, data->gps_quality.satellites_used,
                               GPS_MAX_SATELLITES_IN_USE, data->gps_quality.speed * 3.6,
                               get_gps_quality_string(data));
    }

    return ret;
}

bool gps_is_timeout_detected(void) { return gps_timeout_detected; }