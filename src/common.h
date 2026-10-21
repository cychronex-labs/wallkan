#ifndef WALLKAN_COMMON_H
#define WALLKAN_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_FRAMES_IN_FLIGHT 2

#ifndef COLOR_RESET
    #define COLOR_ERR "\x1b[38;5;196m"
    #define COLOR_WARN "\x1b[38;5;184m"
    #define COLOR_PRIMARY "\x1b[38;5;253m"
    #define COLOR_SECONDARY "\x1b[38;5;238m"
    #define COLOR_TERTIARY "\x1b[38;5;238m"
    #define COLOR_RESET "\x1b[0m"
#endif

static inline uint32_t
bit_pop_lsb_u8(uint8_t *mask)
{
    uint32_t idx = (uint8_t)__builtin_ctz(*mask);
    *mask &= (*mask - 1);
    return idx;
}

static inline uint32_t
bit_pop_lsb_u64(uint64_t *mask)
{
    uint32_t idx = (uint64_t)__builtin_ctzll(*mask);
    *mask &= (*mask - 1);
    return idx;
}


#define bit_pop_lsb(mask_ptr) \
_Generic(*(mask_ptr), uint8_t: bit_pop_lsb_u8, uint64_t: bit_pop_lsb_u64)(mask_ptr)

#define bit_find_free_slot(mask) \
    ({\
       __typeof__(mask) __mask = ~(mask); \
       uint32_t __idx = UINT32_MAX;\
       __mask ? bit_pop_lsb(&__mask) : __idx; \
    })


#define LOG(fmt, ...) \
    fprintf(stderr, COLOR_SECONDARY "[%s:%d] " COLOR_RESET COLOR_PRIMARY fmt COLOR_RESET "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#define WARN(fmt, ...) \
    fprintf(stderr, COLOR_SECONDARY "[%s:%d] " COLOR_RESET COLOR_WARN fmt COLOR_RESET "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#endif
