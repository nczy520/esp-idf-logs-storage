#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化日志管理模块（挂载 SPIFFS，创建初始日志文件）
 * @return true 成功，false 失败
 */
bool logs_spiffs_init(void);

/**
 * @brief 反初始化，关闭文件并卸载 SPIFFS
 */
void logs_spiffs_deinit(void);

/**
 * @brief 写入一条日志（格式化字符串）
 * @param format printf 风格格式字符串
 * @param ... 可变参数
 * @note 线程安全，自动添加时间戳和换行
 */
void logs_spiffs_write(const char *format, ...);

/**
 * @brief 格式化 SPIFFS 分区（擦除所有数据），然后重启系统
 * @return ESP_OK 成功（实际不会返回，因为会重启），否则返回错误码
 */
esp_err_t logs_spiffs_format(void);

#ifdef __cplusplus
}
#endif

