#include <stdio.h>
#include <stdbool.h>

static const char *TAG = "logs_spiffs_example";

void app_main(void)
{
    ESP_LOGI(TAG, "1) init");
    if (!logs_spiffs_init()) {
        ESP_LOGE(TAG, "Failed to initialize logger");
        return;
    }

    ESP_LOGI(TAG, "2) set level");
    logs_spiffs_set_level(LOGS_SPIFFS_LEVEL_WARN);

    ESP_LOGI(TAG, "3) write default log");
    logs_spiffs_write("Hello from logs_spiffs example");

    ESP_LOGI(TAG, "4) write level-specific log");
    logs_spiffs_write_level(LOGS_SPIFFS_LEVEL_ERROR, "This is an error example");

    ESP_LOGI(TAG, "5) rotation config");
    logs_spiffs_rotation_config_t cfg;
    logs_spiffs_rotation_config_default(&cfg);
    logs_spiffs_rotation_config_set(&cfg);
    logs_spiffs_rotation_config_get(&cfg);

    ESP_LOGI(TAG, "6) format");
    (void)logs_spiffs_format();

    ESP_LOGI(TAG, "7) deinit");
    logs_spiffs_deinit();
}
