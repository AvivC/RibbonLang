#ifndef plane_common_h
#define plane_common_h

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

#define DISABLE_GC 0  // Only set to 1 for debugging purposes when you need the GC to not run

/* Generally keep these OFF unless you need them specifically */
#define DEBUG 0 // General debug printing
#define DEBUG_TRACE_EXECUTION 0 // Show stack operations
#define DEBUG_THREADING 0
#define DEBUG_GC 0 // Show GC operations
#define DEBUG_OBJECTS 0 // Show object operations
#define DEBUG_MEMORY_EXECUTION 0 // Show low-level memory operations
#define DEBUG_SCANNER 0 // Show low level lexing output and such
#define DEBUG_PAUSE_AFTER_OPCODES 0 // Wait for user input after each opcode

/* Always leave these two ON in DEV */
#define PRINT_MEMORY_DIAGNOSTICS 1 // Usually leave on in dev. Disable for release
#define DEBUG_IMPORTANT 1 // Pretty much always leave this on, at least in dev - printing critical diagnosis and such

#if DEBUG
    #define DEBUG_PRINT(...) do { \
            fprintf(stdout, "DEBUG: "); \
            fprintf (stdout, __VA_ARGS__); \
            fprintf(stdout, "\n"); \
        } while (false)
            
#else
    #define DEBUG_PRINT(...) do {} while(false)
#endif

#if DEBUG_MEMORY_EXECUTION
    #define DEBUG_MEMORY(...) do { \
            fprintf(stdout, "MEMORY: "); \
            fprintf (stdout, __VA_ARGS__); \
            fprintf(stdout, "\n"); \
        } while (false)

#else
    #define DEBUG_MEMORY(...) do {} while(false)
#endif

#if DEBUG_THREADING
    #define DEBUG_THREADING_PRINT(...) do { \
            fprintf (stdout, __VA_ARGS__); \
        } while (false)

#else
    #define DEBUG_THREADING_PRINT(...) do {} while(false)
#endif

#if DEBUG_IMPORTANT
    #define DEBUG_IMPORTANT_PRINT(...) do { \
            fprintf (stdout, __VA_ARGS__); \
        } while (false)
            
#else
    #define DEBUG_IMPORTANT_PRINT(...) do {} while(false)
#endif


#if DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE(...) do { \
            fprintf (stdout, __VA_ARGS__); \
            fprintf (stdout, "\n"); \
        } while (false)
            
#else
    #define DEBUG_TRACE(...) do {} while(false)
#endif

#if DEBUG_SCANNER
    #define DEBUG_SCANNER_PRINT(...) do { \
            fprintf (stdout, "Scanner: " __VA_ARGS__); \
            fprintf (stdout, "\n"); \
        } while (false)

#else
    #define DEBUG_SCANNER_PRINT(...) do {} while(false)
#endif

#if DEBUG_OBJECTS
    #define DEBUG_OBJECTS_PRINT(...) do { \
            fprintf (stdout, __VA_ARGS__); \
            fprintf (stdout, "\n"); \
        } while (false)

#else
    #define DEBUG_OBJECTS_PRINT(...) do {} while(false)
#endif

#if DEBUG_GC
    #define DEBUG_GC_PRINT(...) do { \
            fprintf (stdout, __VA_ARGS__); \
            fprintf (stdout, "\n"); \
        } while (false)

#else
    #define DEBUG_GC_PRINT(...) do {} while(false)
#endif

// TODO: actual assertion or error mechanisms
#define FAIL(...) do { \
    fprintf(stdout, "\nFAILING! Reason:'"); \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "'\n"); \
    exit(EXIT_FAILURE); \
} while(false)

#endif

#define PRINTLN(str) do { \
		printf("\n"); \
		printf(str); \
		printf("\n"); \
} while (false)

#ifdef _WIN32
#  ifdef _WIN64
#    define PRI_SIZET PRIu64
#  else
#    define PRI_SIZET PRIu32
#  endif
#else
#  define PRI_SIZET "zu"
#endif
