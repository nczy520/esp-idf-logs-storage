#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Web 服务配置
 */
typedef struct {
    bool     auto_start;            /**< 开机自动启动热点（默认 true） */
    uint32_t idle_timeout_ms;       /**< 无连接自动关闭热点超时（默认 3 分钟） */
    uint16_t http_port;             /**< HTTP 服务器端口（默认 8080） */
    uint8_t  ap_channel;            /**< WiFi 通道（默认 1） */
    uint8_t  ap_max_connections;    /**< 最大客户端连接数（默认 4） */
    char      ap_password[16];      /**< WiFi热点密码，WPA2-PSK加密，最多15字符 */
} logs_storage_web_config_t;

/**
 * @brief 获取默认配置
 */
void logs_storage_web_config_default(logs_storage_web_config_t *cfg);

/**
 * @brief 设置当前配置（必须在 logs_storage_web_start 之前调用）
 */
void logs_storage_web_config_set(const logs_storage_web_config_t *cfg);

/**
 * @brief 读取当前配置
 */
void logs_storage_web_config_get(logs_storage_web_config_t *cfg);

#ifdef __cplusplus
}
#endif
