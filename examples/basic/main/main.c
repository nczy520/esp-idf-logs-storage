#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "logs_storage.h"
#include "logs_storage_rotation.h"
#include "logs_storage_internal.h"
#include "logs_storage_web_config.h"

static const char *TAG = "MAIN";

/* 模拟传感器标签 */
static const char *s_sensor_tags[] = {
    "TEMP_001", "HUMID_002", "BATT_003", "PRESS_004",
    "FLOW_005", "VOLT_006", "MQTT_007", "SIGN_008",
    "BTN_009", "TIMER_010", "DISP_011", "STOR_012",
    "NET_013", "ALRM_014", "CFG_015", "CH_016",
    "DATA_017", "PWR_018", "LUX_019", "TEMP_020"
};
#define TAG_COUNT (sizeof(s_sensor_tags) / sizeof(s_sensor_tags[0]))

/* 随机字符集用于数据值 */
static const char s_hex_chars[] = "0123456789ABCDEF";

/* 生成十六进制随机字符串 */
static void generate_hex_string(char *dest, int max_len) {
    int len = 1 + (rand() % (max_len - 1)); // 至少1个字符
    for (int i = 0; i < len; i++) {
        dest[i] = s_hex_chars[rand() % 16];
    }
    dest[len] = '\0';
}

/* 生成随机浮点数字符串 */
static void generate_float_string(char *dest, int max_len) {
    int integer = rand() % 9999;
    int decimals = 1 + (rand() % 3); // 1-3位小数
    
    // 生成小数部分
    char decimal_str[8] = {0};
    for (int i = 0; i < decimals; i++) {
        decimal_str[i] = s_hex_chars[rand() % 16];
    }
    decimal_str[decimals] = '\0';
    
    // 单位
    const char *scales[] = {"mV", "mA", "ohm", "kbps"};
    const char *scale = scales[rand() % 4];
    
    snprintf(dest, max_len, "%d.%s %s", integer, decimal_str, scale);
}

/* 生成随机日志数据 */
static void generate_sensor_data(char *dest, int len) {
    const char *tag = s_sensor_tags[rand() % TAG_COUNT];
    int value_type = rand() % 6;
    char value_str[64] = {0};

    switch (value_type) {
        case 0:  // 十六进制数值
            generate_hex_string(value_str, 4);
            snprintf(dest, len, "[%s] DATA:%s=<0x%s>", tag, value_str, value_str);
            break;
        case 1:  // 浮点数值
            generate_float_string(value_str, sizeof(value_str));
            snprintf(dest, len, "[%s] DATA:%s", tag, value_str);
            break;
        case 2:  // 随机计数器
            {
                int counter = rand() % 10000;
                snprintf(dest, len, "[%s] SEQ:%d_cnt", tag, counter);
            }
            break;
        case 3:  // 随机校验和
            {
                int checksum = rand() % 65536;
                snprintf(dest, len, "[%s] CHK:%04X-%04X", tag, checksum, checksum ^ 0xFFFF);
            }
            break;
        case 4:  // 字符串数据
            {
                char hex_val[16];
                generate_hex_string(hex_val, 8);
                snprintf(dest, len, "[%s] STR:%s len=%02d", tag, hex_val, (int)strlen(hex_val));
            }
            break;
        case 5:  // 状态码
            {
                int status = rand() % 16;
                snprintf(dest, len, "[%s] STS:0x%01X NORMAL", tag, status);
            }
            break;
    }
}

/* 写入传感器日志到存储 */
static void write_sensor_log(void) {
    int level_rand = rand() % 100;
    logs_storage_level_t level;
    if (level_rand < 85) {
        level = LOGS_STORAGE_LEVEL_INFO;
    } else if (level_rand < 95) {
        level = LOGS_STORAGE_LEVEL_WARN;
    } else {
        level = LOGS_STORAGE_LEVEL_ERROR;
    }

    char msg[128];
    generate_sensor_data(msg, sizeof(msg));

    logs_storage_write_level(level, "%s", msg);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF Sensor Log Storage + WiFi Demo (SPIFFS backend)");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "Logger initialized with random sensor simulation");
    ESP_LOGI(TAG, "Login to http://<device-ip>:8080 to view logs");

    srand(esp_timer_get_time());

    if (!logs_storage_init()) {
        ESP_LOGE(TAG, "Failed to initialize log storage");
        return;
    }

    /* 开启控制台同步输出，方便调试时查看日志 */
    logs_storage_set_console_output(true);
    ESP_LOGI(TAG, "Console output enabled - logs will be printed to serial");

    /* 先写入几条测试日志，确保有文件可显示 */
    ESP_LOGI(TAG, "Writing initial sensor logs...");
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "INIT_00000%02d", i);
        logs_storage_write_level(LOGS_STORAGE_LEVEL_INFO, "INIT: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(300)); // 初始日志间隔 300ms
    }

    /* 启动 WiFi 热点 + Web 服务器（默认 3 分钟无连接自动关闭） */
    logs_storage_web_config_t web_cfg;
    logs_storage_web_config_default(&web_cfg);
    /* 可在此覆盖默认配置：
     * web_cfg.auto_start = true;
     * web_cfg.idle_timeout_ms = 3 * 60 * 1000;
     * web_cfg.http_port = 8080;
     * web_cfg.ap_channel = 6;
     * web_cfg.ap_max_connections = 5;
     */
    logs_storage_web_config_set(&web_cfg);

    if (web_cfg.auto_start) {
        logs_storage_web_start();
    }

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "Sensor logging started! Random sensor data every 1s");
    ESP_LOGI(TAG, "Use logs_storage_set_level() to filter verbosity");
    ESP_LOGI(TAG, "View logs at: http://<device-ip>:8080 (or http://10.0.0.1:8080)");
    ESP_LOGI(TAG, "============================================");

    while (true) {
        write_sensor_log();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
