#ifndef plane_common_h
#define plane_common_h

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define DEBUG 0 // General debug printing
#define DEBUG_TRACE_EXECUTION 0 // Show low-level stack and object operations
#define DEBUG_MEMORY_EXECUTION 0 // Show low-level memory operations
#define DEBUG_IMPORTANT 1 // Pretty much always leave this on, at least in dev - printing critical diagnosis and such
#define DEBUG_SCANNER 0 // Show low level lexing output and such

#if DEBUG
    #define DEBUG_PRINT(...) do { \
            fprintf(stderr, "DEBUG: "); \
            fprintf (stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while (false)
            
#else
    #define DEBUG_PRINT(...) do {} while(false)
#endif

#if DEBUG_MEMORY_EXECUTION
    #define DEBUG_MEMORY(...) do { \
            fprintf(stderr, "MEMORY: "); \
            fprintf (stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while (false)

#else
    #define DEBUG_MEMORY(...) do {} while(false)
#endif

#if DEBUG_IMPORTANT
    #define DEBUG_IMPORTANT_PRINT(...) do { \
            fprintf (stderr, __VA_ARGS__); \
        } while (false)
            
#else
    #define DEBUG_IMPORTANT_PRINT(...) do {} while(false)
#endif


#if DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE(...) do { \
            fprintf (stderr, __VA_ARGS__); \
            fprintf (stderr, "\n"); \
        } while (false)
            
#else
    #define DEBUG_TRACE(...) do {} while(false)
#endif

#if DEBUG_SCANNER
    #define DEBUG_SCANNER_PRINT(...) do { \
            fprintf (stderr, "Scanner: " __VA_ARGS__); \
            fprintf (stderr, "\n"); \
        } while (false)

#else
    #define DEBUG_SCANNER_PRINT(...) do {} while(false)
#endif

#define DEBUG_OBJECTS(...) DEBUG_TRACE("Objects: " __VA_ARGS__)

// TODO: actual assertion or error mechanisms
#define FAIL(...) do { \
    fprintf(stderr, "FAILING! Reason:'"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "'\n"); \
    exit(EXIT_FAILURE); \
} while(false)

#endif
