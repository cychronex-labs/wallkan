#ifndef WALLKAN_COMMON_H
#define WALLKAN_COMMON_H

#ifndef COLOR_RESET
    #define COLOR_ERR "\x1b[38;5;196m"
    #define COLOR_WARN "\x1b[38;5;184m"
    #define COLOR_PRIMARY "\x1b[38;5;253m"
    #define COLOR_SECONDARY "\x1b[38;5;238m"
    #define COLOR_TERTIARY "\x1b[38;5;238m"
    #define COLOR_RESET "\x1b[0m"
#endif

#define LOG(fmt, ...) \
    fprintf(stderr, COLOR_SECONDARY "[%s:%d] " COLOR_RESET COLOR_PRIMARY fmt COLOR_RESET "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#define WARN(fmt, ...) \
    fprintf(stderr, COLOR_SECONDARY "[%s:%d] " COLOR_RESET COLOR_WARN fmt COLOR_RESET "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#endif
