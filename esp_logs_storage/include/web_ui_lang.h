/**
 * @file web_ui_lang.h
 * @brief Web UI 语言定义
 * 
 * 支持中英文切换，自动检测系统语言
 * 通过编译器预定义宏 __LOCALE_NAME 或 __LANG 判断
 * 默认使用中文
 */
#ifndef WEB_UI_LANG_H
#define WEB_UI_LANG_H

#include <string.h>

/* 自动检测系统语言
 * 检查编译器预定义宏或环境变量
 * 如果检测到英文环境，则使用英文
 */
#ifndef UI_LANG_EN
    /* 检查各种可能的语言环境指示器 */
    #if defined(__LANG) && (__LANG == 0x656E)  /* 'en' in hex */
        #define UI_LANG_EN
    #elif defined(__LOCALE_NAME)
        #if (__LOCALE_NAME[0] == 'e' && __LOCALE_NAME[1] == 'n')
            #define UI_LANG_EN
        #endif
    #elif defined(__LANGUAGE_ENGLISH)
        #define UI_LANG_EN
    #elif defined(__GNUC__)
        /* GCC: 检查 _POSIX_C_SOURCE 或其他宏 */
        #if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
            /* 默认使用中文，除非明确指定英文 */
        #endif
    #endif
#endif

/* 文本宏定义 */
#ifdef UI_LANG_EN
    #define TXT_TITLE "ESP Log Storage"
    #define TXT_ACCESS_ADDR "Access Address"
    #define TXT_STORAGE_STATS "Storage Stats"
    #define TXT_TOTAL_CAPACITY "Total Capacity"
    #define TXT_USED "Used"
    #define TXT_FREE "Free Space"
    #define TXT_FILE_COUNT "File Count"
    #define TXT_FILE_LIST "Log File List"
    #define TXT_FILENAME "Filename"
    #define TXT_SIZE "Size"
    #define TXT_OPERATION "Operation"
    #define TXT_DOWNLOAD "Download"
    #define TXT_CLEAR_LOGS "Clear All Logs"
    #define TXT_RESTART "Restart Device"
    #define TXT_REFRESH "Refresh List"
    #define TXT_LOADING "Loading..."
    #define TXT_NO_FILES "No log files"
    #define TXT_CONFIRM_CLEAR "Are you sure you want to clear all logs and restart?"
    #define TXT_CLEARED "Logs cleared, device restarting..."
    #define TXT_CONFIRM_RESTART "Are you sure you want to restart?"
#else
    #define TXT_TITLE "ESP 日志存储"
    #define TXT_ACCESS_ADDR "访问地址"
    #define TXT_STORAGE_STATS "存储统计"
    #define TXT_TOTAL_CAPACITY "总容量"
    #define TXT_USED "已使用"
    #define TXT_FREE "剩余空间"
    #define TXT_FILE_COUNT "文件数量"
    #define TXT_FILE_LIST "日志文件列表"
    #define TXT_FILENAME "文件名"
    #define TXT_SIZE "大小"
    #define TXT_OPERATION "操作"
    #define TXT_DOWNLOAD "下载"
    #define TXT_CLEAR_LOGS "清除所有日志"
    #define TXT_RESTART "重启设备"
    #define TXT_REFRESH "刷新列表"
    #define TXT_LOADING "加载中..."
    #define TXT_NO_FILES "暂无日志文件"
    #define TXT_CONFIRM_CLEAR "确定要清除所有日志并重启设备吗？"
    #define TXT_CLEARED "日志已清除，设备正在重启..."
    #define TXT_CONFIRM_RESTART "确定要重启设备吗？"
#endif

#endif /* WEB_UI_LANG_H */
