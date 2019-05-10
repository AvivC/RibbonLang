#ifndef plane_common_h
#define plane_common_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define DEBUG_TRACE_EXECUTION 1

#define DEBUG 1

#if DEBUG
    #define DEBUG_PRINT(...) do { \
            fprintf(stderr, "DEBUG: "); \
            fprintf (stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while (false)
            
#else
    #define DEBUG_PRINT(...) do {} while(false)
#endif

#endif