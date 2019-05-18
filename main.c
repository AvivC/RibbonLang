#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "value.h"
#include "object.h"
#include "vm.h"
#include "memory.h"

static char* readFile(const char* path) {
    // TODO: read relative / absolute paths, etc. Currently only relative to the CWD (which changes in the test runner, etc.)
    
    FILE* file = fopen(path, "rb");
    
    if (file == NULL) {
        fprintf(stderr, "Couldn't open file '%s'. Error:\n'", path);
        perror("fopen");
        fprintf(stderr, "'\n");
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(sizeof(char) * fileSize + 1);
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    
    if (bytesRead != fileSize) {
        fprintf(stderr, "Couldn't read entire file. File size: %d. Bytes read: %d.\n", fileSize, bytesRead);
        return NULL;
    }
    
    if (fclose(file) != 0) {
        fprintf(stderr, "Couldn't close file.\n");
        return NULL;
    }
    
    buffer[fileSize] = '\0';
    return buffer;
}

static void printStructures(int argc, char* argv[], Chunk* chunk, AstNode* ast) {
    bool showBytecode = ((argc == 3) && (strncmp(argv[2], "-asm", 4) == 0))
                        || ((argc == 4) && ((strncmp(argv[2], "-asm", 4) == 0) || strncmp(argv[3], "-asm", 4) == 0));
    
    bool showTree = ((argc == 3) && (strncmp(argv[2], "-tree", 5) == 0))
                    || ((argc == 4) && ((strncmp(argv[2], "-tree", 5) == 0) || strncmp(argv[3], "-tree", 5) == 0));
    
    if (showTree) {
        printf("==== AST ====\n\n");
        printTree(ast);
        printf("\n");
    }
    
    if (showBytecode) {
        printf("==== Bytecode ====\n\n");
        disassembleChunk(chunk);
        printf("\n");
    }
    
    printf("================\n\n");
}

static void printMemoryDiagnostic() {
    printf("\n======== Memory diagnostics ========");
    
    bool problem = false;
    
    size_t allocatedMemory = getAllocatedMemory();
    if (allocatedMemory == 0) {
        DEBUG_IMPORTANT_PRINT("\n*******\nAll memory freed.\n*******");
    } else {
        DEBUG_IMPORTANT_PRINT("\n*******\nAllocated memory is %d, not 0!\n*******", allocatedMemory);
        problem = true;
    }
    
    size_t numAllocations = getAllocationsCount();
    if (numAllocations == 0) {
        DEBUG_IMPORTANT_PRINT("\n*******\nAll allocations freed.\n*******");
    } else {
        DEBUG_IMPORTANT_PRINT("\n*******\nNumber of allocations which have not been freed is %d, not 0!\n*******", numAllocations);
        problem = true;
    }
    
    if (problem) {
        printAllocationsBuffer();
    }
    
    printf("\n======== End memory diagnostics ========\n");
}

int main(int argc, char* argv[]) {
    printf("\n");
    
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: plane <file> [-asm] [-tree]");
        return -1;
    }
    
    char* source = readFile(argv[1]);
    if (source == NULL) {
        return -1;
    }
    
    DEBUG_PRINT("Starting CPlane!\n\n");

    Chunk chunk;
    initChunk(&chunk);
    AstNode* ast = parse(source);
    compile(ast, &chunk);
    
    if (argc > 2) {
        printStructures(argc, argv, &chunk, ast);
    }
    
    initVM(&chunk);
    InterpretResult result = interpret();
    
    freeTree(ast);
    freeVM();
    free(source);
    
    #if DEBUG_IMPORTANT
    printMemoryDiagnostic();
    #endif
    
    return 0; 
}
