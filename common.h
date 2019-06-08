#ifndef plane_common_h
#define plane_common_h

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define DEBUG_TRACE_EXECUTION 0
#define DEBUG 0
#define DEBUG_IMPORTANT 1  // Pretty much always leave this on
#define DEBUG_MEMORY 0

#if DEBUG
    #define DEBUG_PRINT(...) do { \
            fprintf(stderr, "DEBUG: "); \
            fprintf (stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while (false)
            
#else
    #define DEBUG_PRINT(...) do {} while(false)
#endif

#if DEBUG_IMPORTANT
    #define DEBUG_IMPORTANT_PRINT(...) do { \
            fprintf (stderr, __VA_ARGS__); \
        } while (false)
            
#else
    #define DEBUG_IMPORTANT_PRINT(...) do {} while(false)
#endif

#if DEBUG_MEMORY
    #define DEBUG_MEMORY_PRINT(...) do { \
            fprintf (stderr, __VA_ARGS__); \
        } while (false)
            
#else
    #define DEBUG_MEMORY_PRINT(...) do {} while(false)
#endif

#if DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE(...) do { \
            fprintf (stderr, __VA_ARGS__); \
        } while (false)
            
#else
    #define DEBUG_TRACE(...) do {} while(false)
#endif

// TODO: actual assertion or error mechanisms
#define FAIL(...) do { \
    fprintf(stderr, "FAILING! Reason:'"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "'\n"); \
    exit(EXIT_FAILURE); \
} while(false)

#endif
