#ifndef WALLKAN_ERR_H
#define WALLKAN_ERR_H

typedef enum WkResult {
    WK_OK,
} WkResult;

WkResult
wk_log_error(WkResult code, const char *code_name, const char *file, int line, const char *fmt,...);

#define WK_ERR(code, fmt, ...) \
    wk_log_error(code, #code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define WK_TRY(expr) \
    do {\
        WkResult _res = (expr);\
        if(_res != RESULT_OK) return _res;\
    } while(0)

#endif
