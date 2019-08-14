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

// Generally this will always be false. Only set to true when experimenting during development
#define TRYING_THINGS false

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

static bool checkCmdArg(char** argv, int argc, int index, const char* value) {
	return argc >= index + 1 && strncmp(argv[index], value, strlen(value)) == 0;
}

static bool cmdArgExists(char** argv, int argc, const char* value) {
	for (int i = 0; i < argc; i++) {
		if (checkCmdArg(argv, argc, i, value)) {
			return true;
		}
	}

	return false;
}

static void printStructures(int argc, char* argv[], Chunk* chunk, AstNode* ast) {
    bool showBytecode = cmdArgExists(argv, argc, "-asm");
    bool showTree = cmdArgExists(argv, argc, "-tree");
    
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
    
    printf("================\n");
}

static void printMemoryDiagnostic() {
    printf("======== Memory diagnostics ========");
    
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
    
    printf("\n*******\n");
    printAllObjects();
    printf("*******\n");

    printf("======== End memory diagnostics ========\n");
}

int main(int argc, char* argv[]) {
	// To make experimenting while working easy
	if (TRYING_THINGS) {
		printf("--- Experimentation mode. ---\n");
		return 0;
	}

    if (argc < 2 || argc > 5) {
        fprintf(stderr, "Usage: plane <file> [[-asm] [-tree] [-dry]]");
        return -1;
    }
    
    char* source = readFile(argv[1]);
    if (source == NULL) {
        return -1;
    }
    
    DEBUG_PRINT("Starting CPlane!\n\n");

    // Must first init the VM, because some parts of the compiler depend on it
    initVM();

    Chunk chunk;
    initChunk(&chunk);
    AstNode* ast = parse(source);
    compile(ast, &chunk);
    
    if (argc > 2) {
        printStructures(argc, argv, &chunk, ast);
    }
    
    bool dryRun = checkCmdArg(argv, argc, 2, "-dry") || checkCmdArg(argv, argc, 3, "-dry") || checkCmdArg(argv, argc, 4, "-dry");
    if (!dryRun) {
    	InterpretResult result = interpret(&chunk);
    }
    
    freeTree(ast);
    freeVM();
    free(source);
    
    #if DEBUG_IMPORTANT
    if (!dryRun) {
    	// Dry running does no GC, so some things from the compiler aren't cleaned... So no point in printing diagnostics.
    	// Not ideal, but leave this for now
    	printMemoryDiagnostic();
    }
    #endif
    
    return 0; 
}
