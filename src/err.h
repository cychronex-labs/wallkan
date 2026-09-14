#ifndef WALLKAN_ERR_H
#define WALLKAN_ERR_H
#include <stdarg.h>

typedef enum Result {
    RESULT_OK,
} Result;

Result
log_error(Result code, const char *code_name, const char *file, int line, const char *fmt,...);

#define ERR(code, fmt, ...) \
    log_error(code, #code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define TRY(expr) \
    do {\
        Result _res = (expr);\
        if(_res != RESULT_OK) return _res;\
    } while(0)

#endif
