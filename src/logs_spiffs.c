#include "logs_spiffs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LOG_MGR";

// ========== 配置参数 ==========
#define SPIFFS_BASE_PATH      "/spiffs"
#define LOG_FILE_PREFIX       "log_"
#define LOG_FILE_EXT          ".log"
#define MAX_LOG_FILES         99
#define MIN_FREE_SPACE_BYTES  256 * 1024   // 保留至少 256 KiB 空闲空间
#define MAX_FILE_SIZE_BYTES   512 * 1024   // 单文件超过 512KB 触发轮转
#define FILENAME_NUM_DIGITS   6

static FILE *s_current_log_file = NULL;
static char s_current_log_path[64];
static SemaphoreHandle_t s_log_mutex = NULL;

static bool rotate_log_file_unlocked(void);
static bool ensure_free_space_unlocked(size_t required_bytes);
static void enforce_max_file_count_unlocked(void);
static int delete_oldest_logs_unlocked(int keep_count, size_t need_free_bytes);
static int get_next_log_number_unlocked(void);
static void list_existing_log_files(void);

// ========== 辅助函数：文件序号解析与排序 ==========
static int extract_log_number(const char *filename) {
    if (!filename || strncmp(filename, LOG_FILE_PREFIX, strlen(LOG_FILE_PREFIX)) != 0) {
        return -1;
    }
    const char *num_start = filename + strlen(LOG_FILE_PREFIX);
    char *endptr;
    long num = strtol(num_start, &endptr, 10);
    if (endptr == num_start || strcmp(endptr, LOG_FILE_EXT) != 0) {
        return -1;
    }
    return (int)num;
}

// 排序比较函数：直接比较数字，确保升序
static int compare_log_number(const void *a, const void *b) {
    const char *path_a = *(const char **)a;
    const char *path_b = *(const char **)b;
    const char *name_a = strrchr(path_a, '/');
    const char *name_b = strrchr(path_b, '/');
    name_a = name_a ? name_a + 1 : path_a;
    name_b = name_b ? name_b + 1 : path_b;
    int num_a = extract_log_number(name_a);
    int num_b = extract_log_number(name_b);
    if (num_a < 0 || num_b < 0) {
        return strcmp(name_a, name_b);
    }
    return num_a - num_b;
}

static esp_err_t get_sorted_log_files_unlocked(char ***out_paths, int *out_count) {
    DIR *dir = opendir(SPIFFS_BASE_PATH);
    if (!dir) {
        *out_paths = NULL;
        *out_count = 0;
        return ESP_ERR_NOT_FOUND;
    }

    char **files = NULL;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *name = entry->d_name;
            if (extract_log_number(name) >= 0) {
                char *full_path = malloc(strlen(SPIFFS_BASE_PATH) + strlen(name) + 2);
                if (!full_path) {
                    for (int i = 0; i < count; i++) free(files[i]);
                    free(files);
                    closedir(dir);
                    *out_paths = NULL;
                    *out_count = 0;
                    return ESP_ERR_NO_MEM;
                }
                sprintf(full_path, "%s/%s", SPIFFS_BASE_PATH, name);

                char **tmp = realloc(files, (count + 1) * sizeof(char *));
                if (!tmp) {
                    free(full_path);
                    for (int i = 0; i < count; i++) free(files[i]);
                    free(files);
                    closedir(dir);
                    *out_paths = NULL;
                    *out_count = 0;
                    return ESP_ERR_NO_MEM;
                }
                files = tmp;
                files[count++] = full_path;
            }
        }
    }
    closedir(dir);

    if (count == 0) {
        free(files);
        *out_paths = NULL;
        *out_count = 0;
        return ESP_OK;
    }

    qsort(files, count, sizeof(char *), compare_log_number);
    *out_paths = files;
    *out_count = count;
    return ESP_OK;
}

// ---------- 删除旧文件 ----------
static int delete_oldest_logs_unlocked(int keep_count, size_t need_free_bytes) {
    char **files = NULL;
    int file_count = 0;
    if (get_sorted_log_files_unlocked(&files, &file_count) != ESP_OK || file_count == 0) {
        if (files) free(files);
        return 0;
    }

    int deleted = 0;
    size_t freed_bytes = 0;
    int files_to_delete = file_count - keep_count;
    if (files_to_delete <= 0) {
        for (int i = 0; i < file_count; i++) free(files[i]);
        free(files);
        return 0;
    }

    // 确保至少保留 keep_count 个，且释放足够空间
    for (int i = 0; i < file_count; i++) {
        // 如果保留文件数已达 keep_count 且已释放足够空间，则停止
        if ((file_count - deleted) <= keep_count && freed_bytes >= need_free_bytes) {
            break;
        }
        // 如果待删文件数已用完，停止
        if (deleted >= files_to_delete && freed_bytes >= need_free_bytes) {
            break;
        }

        struct stat st;
        size_t file_size = 0;
        if (stat(files[i], &st) == 0) {
            file_size = st.st_size;
        }
        if (remove(files[i]) == 0) {
            freed_bytes += file_size;
            deleted++;
            ESP_LOGI(TAG, "Deleted old log: %s (%ld bytes)", files[i], (long)file_size);
        } else {
            ESP_LOGW(TAG, "Failed to delete %s", files[i]);
        }
        free(files[i]);
    }

    // 释放剩余未处理的指针
    for (int i = deleted; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
    return deleted;
}

// ---------- 确保空闲空间 ----------
static bool ensure_free_space_unlocked(size_t required_bytes) {
    size_t total = 0, used = 0;
    if (esp_spiffs_info(NULL, &total, &used) != ESP_OK) return false;
    size_t free_bytes = total - used;
    if (free_bytes >= required_bytes) return true;

    // 需要释放的空间 = required_bytes - free_bytes
    size_t need_to_free = required_bytes - free_bytes;
    // 至少保留 MAX_LOG_FILES 个文件，但也要尽量释放
    // 使用 delete_oldest_logs_unlocked 删除最旧文件直到满足空间需求
    // 但我们无法预先知道删除多少个，故循环尝试
    int prev_deleted = -1;
    while (free_bytes < required_bytes) {
        // 获取当前文件列表，了解数量
        char **files = NULL;
        int file_count = 0;
        if (get_sorted_log_files_unlocked(&files, &file_count) != ESP_OK) {
            if (files) free(files);
            break;
        }
        // 释放临时列表，只用于计数
        for (int i = 0; i < file_count; i++) free(files[i]);
        free(files);

        if (file_count == 0) break;
        // 若文件数已不超过 MAX_LOG_FILES，则不能再删（保持最多）
        if (file_count <= MAX_LOG_FILES && prev_deleted == 0) {
            break; // 无法删除更多（实际上 keep_count = MAX_LOG_FILES）
        }

        // 删除足够文件，但保留最多 MAX_LOG_FILES 个
        int deleted = delete_oldest_logs_unlocked(MAX_LOG_FILES, need_to_free);
        if (deleted == 0) break; // 没有删除任何文件

        if (esp_spiffs_info(NULL, &total, &used) != ESP_OK) break;
        free_bytes = total - used;
        need_to_free = required_bytes - free_bytes;
        if (need_to_free <= 0) break;

        // 避免死循环
        if (deleted == prev_deleted) break;
        prev_deleted = deleted;
    }
    return free_bytes >= required_bytes;
}

// ---------- 强制限制文件数 ----------
static void enforce_max_file_count_unlocked(void) {
    delete_oldest_logs_unlocked(MAX_LOG_FILES, 0);
}

// ---------- 获取下一个可用编号 ----------
static int get_next_log_number_unlocked(void) {
    char **files = NULL;
    int file_count = 0;
    int max_num = 0;
    if (get_sorted_log_files_unlocked(&files, &file_count) == ESP_OK) {
        for (int i = 0; i < file_count; i++) {
            const char *name = strrchr(files[i], '/');
            name = name ? name + 1 : files[i];
            int num = extract_log_number(name);
            if (num > max_num) max_num = num;
            free(files[i]);
        }
        free(files);
    }
    // 如果 max_num 超过安全范围（如 > 999999），重置为 0 避免溢出
    if (max_num >= 999999) {
        max_num = 0;
    }
    return max_num + 1;
}

// ---------- 生成日志路径 ----------
static void generate_log_path(char *out_path, int number) {
    sprintf(out_path, "%s/%s%0*d%s", SPIFFS_BASE_PATH, LOG_FILE_PREFIX,
            FILENAME_NUM_DIGITS, number, LOG_FILE_EXT);
}

// ---------- 轮转日志文件 ----------
static bool rotate_log_file_unlocked(void) {
    if (s_current_log_file) {
        fclose(s_current_log_file);
        s_current_log_file = NULL;
    }

    // 先检查空间是否足够创建新文件（预留至少8KB）
    if (!ensure_free_space_unlocked(8 * 1024)) {
        ESP_LOGE(TAG, "Cannot create new log file, insufficient space");
        return false;
    }

    // 确保文件数量不超限（删除最旧文件）
    enforce_max_file_count_unlocked();

    int next_num = get_next_log_number_unlocked();
    generate_log_path(s_current_log_path, next_num);
    s_current_log_file = fopen(s_current_log_path, "a");
    if (!s_current_log_file) {
        ESP_LOGE(TAG, "Failed to create log file: %s", s_current_log_path);
        return false;
    }

    setvbuf(s_current_log_file, NULL, _IOLBF, 256);
    ESP_LOGI(TAG, "New log file: %s (number %d)", s_current_log_path, next_num);
    return true;
}

// ---------- 列出现有日志文件 ----------
static void list_existing_log_files(void) {
    char **files = NULL;
    int file_count = 0;
    if (get_sorted_log_files_unlocked(&files, &file_count) != ESP_OK || file_count == 0) {
        ESP_LOGI(TAG, "No log files found");
        if (files) free(files);
        return;
    }

    printf("========== Existing log files ==========\n");
    ESP_LOGI(TAG, "Existing log files (sorted by number):");
    for (int i = 0; i < file_count; i++) {
        const char *path = files[i];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        struct stat st;
        if (stat(path, &st) == 0) {
            printf("  /spiffs/%s  (%ld bytes)\n", name, (long)st.st_size);
        } else {
            printf("  /spiffs/%s  (size unknown)\n", name);
        }
        free(files[i]);
    }
    free(files);
    printf("========================================\n");
}

// ========== 公共接口 ==========
esp_err_t logs_spiffs_format(void) {
    ESP_LOGI(TAG, "Formatting SPIFFS partition...");
    esp_err_t ret = esp_spiffs_format(NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Format successful! Restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Format failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool logs_spiffs_init(void) {
    s_log_mutex = xSemaphoreCreateMutex();
    if (s_log_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        vSemaphoreDelete(s_log_mutex);
        s_log_mutex = NULL;
        return false;
    }

    size_t total = 0, used = 0;
    if (esp_spiffs_info(NULL, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted, total: %d KB, used: %d KB", (int)(total/1024), (int)(used/1024));
    }

    if (xSemaphoreTake(s_log_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex during init");
        esp_vfs_spiffs_unregister(NULL);
        vSemaphoreDelete(s_log_mutex);
        s_log_mutex = NULL;
        return false;
    }

    list_existing_log_files();

    if (!rotate_log_file_unlocked()) {
        ESP_LOGE(TAG, "Failed to create initial log file");
        xSemaphoreGive(s_log_mutex);
        esp_vfs_spiffs_unregister(NULL);
        vSemaphoreDelete(s_log_mutex);
        s_log_mutex = NULL;
        return false;
    }

    xSemaphoreGive(s_log_mutex);

    logs_spiffs_write("[SYSTEM] System started, SPIFFS total: %d KB, used: %d KB",
            (int)(total/1024), (int)(used/1024));

    return true;
}

void logs_spiffs_write(const char *format, ...) {
    if (s_log_mutex == NULL) return;

    if (xSemaphoreTake(s_log_mutex, portMAX_DELAY) != pdTRUE) return;

    // 检查并确保当前文件有效
    if (!s_current_log_file) {
        if (!rotate_log_file_unlocked()) {
            xSemaphoreGive(s_log_mutex);
            return;
        }
    }

    // 检查空闲空间
    if (!ensure_free_space_unlocked(MIN_FREE_SPACE_BYTES)) {
        ESP_LOGW(TAG, "Insufficient space, skipping log write");
        xSemaphoreGive(s_log_mutex);
        return;
    }

    // 检查当前文件大小，若超过阈值则轮转
    long cur_pos = ftell(s_current_log_file);
    if (cur_pos > MAX_FILE_SIZE_BYTES) {
        ESP_LOGI(TAG, "Log file size %ld bytes, rotating", cur_pos);
        if (!rotate_log_file_unlocked()) {
            ESP_LOGE(TAG, "Rotation failed, skipping log");
            xSemaphoreGive(s_log_mutex);
            return;
        }
    }

    int64_t ms = esp_timer_get_time() / 1000;
    fprintf(s_current_log_file, "[+%lld ms] ", (long long)ms);

    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);
    vfprintf(s_current_log_file, format, args);
    fprintf(s_current_log_file, "\n");
    fflush(s_current_log_file);   // 确保数据落盘
    va_end(args);

    // 同时输出到控制台
    printf("[+%lld ms] ", (long long)ms);
    vprintf(format, args_copy);
    printf("\n");
    va_end(args_copy);

    xSemaphoreGive(s_log_mutex);
}

void logs_spiffs_deinit(void) {
    if (s_log_mutex == NULL) return;

    if (xSemaphoreTake(s_log_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_current_log_file) {
            fflush(s_current_log_file);
            fclose(s_current_log_file);
            s_current_log_file = NULL;
        }
        esp_vfs_spiffs_unregister(NULL);
        xSemaphoreGive(s_log_mutex);
        vSemaphoreDelete(s_log_mutex);
        s_log_mutex = NULL;
    }
}