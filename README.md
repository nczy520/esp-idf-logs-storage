# esp-idf-logs-storage

[![Component](https://img.shields.io/badge/ESP--IDF-6.0%2B-blue)](https://docs.espressif.com/projects/esp-idf/)
[![License](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![Target](https://img.shields.io/badge/target-ESP32--S2%2FS3%2FC5%2FC6%2FH2-orange)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Version](https://img.shields.io/badge/version-1.1.2-blue)](https://github.com/nczy520/esp-idf-logs-storage)

一个面向 ESP-IDF 的 SPIFFS 日志组件，提供自动轮转、空间控制和线程安全写入能力，适合在设备端把运行日志落盘到 SPIFFS 分区。

## 项目结构

```
esp-idf-logs-storage/
├── esp_logs_storage/         # 组件源码（ESP-IDF 组件）
│   ├── include/              # 公共头文件
│   │   ├── logs_storage.h              # 公共 API
│   │   ├── logs_storage_internal.h     # 内部数据结构
│   │   ├── logs_storage_rotation.h     # 轮转配置
│   │   ├── logs_storage_web_config.h   # Web 服务配置
│   │   ├── web_constants.h             # Web 常量
│   │   └── web_ui_lang.h               # UI 语言支持
│   ├── src/
│   │   ├── core/             # 日志存储核心模块
│   │   │   ├── logs_storage.c            # 公共 API 入口
│   │   │   ├── logs_storage_backend.c    # SPIFFS 后端
│   │   │   ├── logs_storage_file_utils.c # 文件工具
│   │   │   ├── logs_storage_worker.c     # Worker 任务
│   │   │   ├── logs_storage_rotation.c   # 轮转配置
│   │   │   └── logs_storage_web_config.c # Web 配置
│   │   └── web/              # Web 服务模块
│   │       ├── web_main.c              # HTTP 服务器主控
│   │       ├── web_wifi.c              # WiFi AP 管理
│   │       ├── web_api.c               # RESTful API
│   │       ├── web_ui.c                # Web UI 页面
│   │       └── web_dns_server.c        # DNS 服务器
│   ├── CMakeLists.txt
│   ├── Kconfig               # menuconfig 配置
│   └── idf_component.yml
├── examples/
│   └── basic/                # 示例工程
│       ├── main/
│       ├── CMakeLists.txt
│       ├── partitions.csv
│       └── sdkconfig.defaults
├── LICENSE
└── README.md
```

## 组件特性

### 日志存储核心
- 自动创建并轮转日志文件
- 超过大小时自动切换文件
- 低空间时自动清理旧日志
- 通过队列和后台工作线程串行化写入，避免多线程同时写入 SPIFFS 文件造成竞争
- 批量写入（最多 8 条或 200ms 触发一次 flush），降低 SPIFFS 写放大开销
- 日志级别过滤（INFO / WARN / ERROR）
- 代码按模块拆分，便于维护：存储层、工作线程层、公共接口层
- 可直接作为 ESP-IDF 组件被其他项目引用

### Web 服务（新增）
- **WiFi AP 模式**：设备启动时自动创建 WiFi 热点
- **DNS 劫持**：将所有域名解析指向设备 IP，实现 Captive Portal
- **HTTP 服务器**：提供 RESTful API 和 Web 管理界面
- **Captive Portal 支持**：自动检测 iOS/Android/Windows 设备，弹出登录页面
- **Web UI**：响应式网页，支持移动端访问
  - 实时显示存储统计信息
  - 日志文件列表查看和下载
  - 一键清除所有日志
  - 设备重启控制
- **多语言支持**：自动检测系统语言，支持中文/英文切换
- **menuconfig 配置**：所有参数可通过 menuconfig 配置

## 在项目中使用

在你的应用项目的 `idf_component.yml` 中添加依赖：

```yaml
dependencies:
  esp_logs_storage:
    git: https://github.com/nczy520/esp-idf-logs-storage.git
    path: esp_logs_storage
    branch: main
```

然后在应用代码中调用：

```c
#include "logs_storage.h"
#include "logs_storage_rotation.h"

void app_main(void) {
    logs_storage_rotation_config_t cfg;
    logs_storage_rotation_config_default(&cfg);
    cfg.max_file_size_bytes = 256u * 1024u;
    cfg.max_log_files = 10u;
    cfg.min_free_space_bytes = 128u * 1024u;
    cfg.rotate_threshold_bytes = 32u * 1024u;
    logs_storage_rotation_config_set(&cfg);

    if (!logs_storage_init()) {
        return;
    }

    logs_storage_set_level(LOGS_STORAGE_LEVEL_WARN);
    logs_storage_write("This is an INFO log, it will be filtered out");
    logs_storage_write_level(LOGS_STORAGE_LEVEL_WARN, "This warning is persisted");
    logs_storage_write_level(LOGS_STORAGE_LEVEL_ERROR, "This error is persisted");
}
```

### 说明

- `logs_storage_set_level()` 用来设置最低记录级别，低于该级别的消息会被直接丢弃。该接口是线程安全的（原子读写）。
- `logs_storage_write()` 会写入默认级别的 INFO 日志。
- `logs_storage_write_level()` 可显式指定 `INFO/WARN/ERROR`。
- `logs_storage_rotation_config_*()` 可以调整轮转阈值、最大文件大小、保留文件数和最低剩余空间。
- `logs_storage_format()` 会擦除整个 storage 分区（SPIFFS 格式化），成功返回 `ESP_OK`，失败返回相应 `esp_err_t`。调用前后建议先 `logs_storage_deinit()`、格式化后再 `logs_storage_init()` 重新挂载。

### Web 服务配置

通过 menuconfig 配置 Web 服务：

```bash
idf.py menuconfig
# Component config -> ESP Log Storage Configuration
```

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| Auto-start Web Server | 启用 | 开机自动启动 Web 服务器 |
| Web Server Port | 8080 | HTTP 服务器端口 |
| WiFi AP SSID Prefix | LOGS | WiFi 热点名称前缀 |
| WiFi AP Password | 12345678 | WiFi 热点密码 |
| WiFi AP Channel | 1 | WiFi 信道 |
| Max WiFi Connections | 4 | 最大连接数 |
| Idle Timeout | 180000 ms | 空闲超时时间 |
| Max Log File Size | 1048576 bytes | 单个日志文件最大尺寸 |
| Max Log File Count | 10 | 保留的日志文件数量上限 |
| Min Free Space | 262144 bytes | 触发清理前的最低剩余空间 |
| Rotate Threshold | 8192 bytes | 触发轮转前的最小可用空间 |

### 控制台日志输出

组件内部所有状态信息通过 `ESP_LOGI/ESP_LOGW/ESP_LOGE` 输出，TAG 为 `LOG_MGR`。落盘日志的同步回显默认关闭（使用 `ESP_LOGD`），如需在串口实时查看写入的日志内容，可启用：

```c
#include "esp_log.h"
esp_log_level_set("LOG_MGR", ESP_LOG_DEBUG);
```

## 示例工程

仓库中已提供一个最小示例，目录为 [examples/basic](examples/basic)。

在 ESP-IDF 环境中运行：

```bash
cd examples/basic
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

示例会演示：

- 设置轮转配置
- 初始化 SPIFFS 日志组件
- 使用不同级别的日志写入
- 观察日志文件被生成和轮转
- 连接 WiFi AP 自动弹出 Web 管理页面

## API

### 日志存储 API

| 函数 | 说明 |
|------|------|
| `logs_storage_init()` | 初始化：挂载 SPIFFS，启动 worker，创建初始日志文件 |
| `logs_storage_deinit()` | 反初始化：停止 worker，关闭文件，卸载 SPIFFS |
| `logs_storage_set_level()` | 设置最低日志级别 |
| `logs_storage_write()` | 写入一条 INFO 级别日志 |
| `logs_storage_write_level()` | 写入一条指定级别日志 |
| `logs_storage_format()` | 格式化 storage 分区（SPIFFS）并返回 `esp_err_t` |
| `logs_storage_rotation_config_default()` | 获取默认轮转配置 |
| `logs_storage_rotation_config_set()` | 设置当前轮转配置 |
| `logs_storage_rotation_config_get()` | 读取当前轮转配置 |

### Web 服务 API

| 函数 | 说明 |
|------|------|
| `logs_storage_web_start()` | 启动 Web 服务（WiFi AP + DNS + HTTP） |
| `logs_storage_web_stop()` | 停止 Web 服务 |

### RESTful API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/stats` | GET | 获取存储空间统计信息 |
| `/api/list` | GET | 列出所有日志文件 |
| `/api/download?file=xxx` | GET | 下载指定日志文件 |
| `/api/clear` | POST | 清除所有日志并重启设备 |
| `/api/restart` | POST | 重启设备 |
| `/api/system` | GET | 获取系统信息 |

### 默认轮转配置

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `min_free_space_bytes` | 256 KB | 触发清理前的最低剩余空间 |
| `max_file_size_bytes` | 1 MB | 单个日志文件最大尺寸 |
| `max_log_files` | 10 | 保留的日志文件数量上限 |
| `rotate_threshold_bytes` | 8 KB | 创建新文件前需保证的最小可用空间 |
| `max_files_open` | 5 | SPIFFS 允许同时打开的文件数量 |

## 许可证

Apache-2.0
