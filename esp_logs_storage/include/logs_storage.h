#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别，值越大级别越高
 */
typedef enum {
    LOGS_STORAGE_LEVEL_DEBUG   = 0,
    LOGS_STORAGE_LEVEL_INFO    = 1,
    LOGS_STORAGE_LEVEL_WARN    = 2,
    LOGS_STORAGE_LEVEL_ERROR   = 3,
    LOGS_STORAGE_LEVEL_NONE    = 4
} logs_storage_level_t;

/**
 * @brief 初始化日志存储
 *
 * 挂载 SPIFFS 文件系统，创建日志文件，启动 worker 任务。
 *
 * @return 成功返回 true，失败返回 false
 */
bool logs_storage_init(void);

/**
 * @brief 反初始化日志存储
 *
 * 停止 worker，关闭日志文件，卸载 SPIFFS。
 */
void logs_storage_deinit(void);

/**
 * @brief 写入一条日志（格式化字符串）
 *
 * @param level 日志级别
 * @param format printf 风格格式字符串
 * @return 成功返回 true，失败（队列满/级别不够）返回 false
 */
bool logs_storage_write_level(logs_storage_level_t level, const char *format, ...);

/**
 * @brief 写入一条已格式化的日志消息
 *
 * @param level 日志级别
 * @param message 完整的日志消息
 * @return 成功返回 true，失败返回 false
 */
bool logs_storage_write(logs_storage_level_t level, const char *message);

/**
 * @brief 设置日志级别过滤阈值
 *
 * 低于此级别的日志将被丢弃。
 *
 * @param level 日志级别
 */
void logs_storage_set_level(logs_storage_level_t level);

/**
 * @brief 获取当前日志级别
 *
 * @return 当前日志级别
 */
logs_storage_level_t logs_storage_get_level(void);

/**
 * @brief 打印所有现存日志文件列表
 */
void logs_storage_list_files(void);

/**
 * @brief 格式化存储分区（清除所有日志）
 *
 * @return ESP_OK 成功，其他失败
 */
esp_err_t logs_storage_format(void);

/**
 * @brief 设置是否在控制台同步输出日志
 *
 * 开启后，写入存储的日志会同时输出到串口控制台。
 *
 * @param enable true 开启同步输出，false 关闭
 */
void logs_storage_set_console_output(bool enable);

/**
 * @brief 获取当前控制台同步输出状态
 *
 * @return true 已开启，false 已关闭
 */
bool logs_storage_get_console_output(void);

#ifdef __cplusplus
}
#endif
