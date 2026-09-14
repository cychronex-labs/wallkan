#include <stdarg.h>
#include <stdio.h>
#include "common.h"
#include "err.h"

WkResult
wk_log_error(WkResult code, const char *code_name, const char *file, int line, const char *fmt,...)
{
    fprintf(stderr, "%sFAILED!\n", COLOR_ERR);
    fprintf(stderr, "    ERROR:    %s (%d)%s\n", code_name, code, COLOR_RESET);
    fprintf(stderr, "    %sAT:       %s:%d%s\n", COLOR_WARN, file, line, COLOR_RESET);
    fprintf(stderr, "    %sMESSAGE: ", COLOR_ERR);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "%s\n", COLOR_RESET);
    return code;
}
