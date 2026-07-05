#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "logs_storage.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 队列/任务配置 */
#define LOGS_STORAGE_QUEUE_LENGTH          64   /**< 操作队列容量（条目数） */
#define LOGS_STORAGE_MAX_MESSAGE_LEN       384  /**< 单条日志最大长度（含 '\0'） */
#define LOGS_STORAGE_TASK_STACK_WORDS      10240 /**< worker 任务栈大小（字） */
#define LOGS_STORAGE_TASK_PRIORITY         5    /**< worker 任务优先级 */
#define LOGS_STORAGE_QUEUE_TIMEOUT_MS      100  /**< 入队超时，超时则丢弃 */
#define LOGS_STORAGE_BATCH_FLUSH_INTERVAL_MS 200 /**< 批量 flush 间隔（ms） */
#define LOGS_STORAGE_BATCH_MAX_ITEMS       8    /**< 批量最大条目数 */

/* 存储路径与文件名常量 - 从menuconfig读取 */
#define STORAGE_BASE_PATH      CONFIG_LOGS_STORAGE_BASE_PATH
#define LOG_FILE_PREFIX        CONFIG_LOGS_STORAGE_FILE_PREFIX
#define LOG_FILE_EXT           CONFIG_LOGS_STORAGE_FILE_EXT
#define FILENAME_NUM_DIGITS    CONFIG_LOGS_STORAGE_FILENAME_DIGITS

/* Worker 操作类型 */
typedef enum {
    OP_WRITE_LOG = 0,    /**< 写入日志消息 */
    OP_GET_STATS,        /**< 获取空间统计 */
    OP_LIST_FILES,       /**< 列出日志文件 */
    OP_CLEAR_ALL,        /**< 清除所有日志 */
    OP_ROTATE,           /**< 触发轮转 */
} logs_storage_op_type_t;

/* 查询结果（用于异步返回数据给请求者） */
typedef struct {
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
} logs_storage_stats_result_t;

/* 队列条目（支持多种操作） */
typedef struct {
    logs_storage_op_type_t op_type;  /**< 操作类型 */
    union {
        /* OP_WRITE_LOG: 写入日志 */
        struct {
            logs_storage_level_t level;
            char message[LOGS_STORAGE_MAX_MESSAGE_LEN];
        } write_log;

        /* OP_GET_STATS: 空间查询 */
        struct {
            logs_storage_stats_result_t *result;
        } get_stats;

        /* OP_LIST_FILES: 列出文件 */
        struct {
            char ***out_paths;
            int *out_count;
        } list_files;

        /* OP_CLEAR_ALL 和 OP_ROTATE: 无额外参数 */
        char padding[1];
    } data;
} logs_storage_queue_item_t;

/* 全局存储上下文（单例，纯队列驱动，无锁） */
typedef struct {
    FILE *current_log_file;                  /**< 当前日志文件句柄 */
    char current_log_path[64];               /**< 当前日志文件路径 */
    QueueHandle_t log_queue;                 /**< 操作队列（唯一的并发控制） */
    TaskHandle_t log_task;                   /**< worker 任务句柄 */
    SemaphoreHandle_t log_task_done;         /**< worker 退出信号量 */
    volatile bool shutdown_requested;        /**< 通知 worker 退出 */
    long current_log_size;                   /**< 当前日志文件大小缓存（用于 Web API） */
} logs_storage_context_t;

extern logs_storage_context_t g_logs_storage;

/* ===== 后端核心（logs_storage_backend.c）===== */
bool logs_storage_backend_init(void);
void logs_storage_backend_deinit(void);
bool logs_storage_rotate_file(void);
void logs_storage_list_existing(void);
esp_err_t logs_storage_backend_format(void);

/* ===== 文件工具（logs_storage_file_utils.c）=====
 * 所有函数由 worker 任务独占调用，不需要任何锁。 */
bool get_storage_free_bytes(const char *path, size_t *out_total, size_t *out_free);
esp_err_t get_sorted_log_files(char ***out_paths, int *out_count);
int  delete_oldest_logs(int keep_count, size_t need_free_bytes);
bool ensure_free_space(size_t required_bytes);
void enforce_max_file_count(void);
void generate_log_path(char *out_path, int number);
int  get_next_log_number(void);

/* ===== Worker 任务（logs_storage_worker.c）===== */
bool logs_storage_worker_start(void);
void logs_storage_worker_stop(void);
bool logs_storage_worker_enqueue_message(logs_storage_level_t level, const char *message);
bool logs_storage_worker_enqueue_formatted(logs_storage_level_t level, const char *format, ...);
bool logs_storage_worker_request_stats(logs_storage_stats_result_t *result);
bool logs_storage_worker_request_list_files(char ***out_paths, int *out_count);
void logs_storage_worker_request_clear_all(void);
void logs_storage_worker_request_rotate(void);
void logs_storage_worker_set_level(logs_storage_level_t level);
logs_storage_level_t logs_storage_worker_get_level(void);
void logs_storage_worker_set_console_output(bool enable);
bool logs_storage_worker_get_console_output(void);

/* ===== Web 服务器 ===== */
void logs_storage_web_start(void);
void logs_storage_web_stop(void);

#ifdef __cplusplus
}
#endif
