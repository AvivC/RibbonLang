#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "io.h"
#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "disassembler.h"
#include "value.h"
#include "object.h"
#include "vm.h"
#include "memory.h"

// Generally this will always be false. Only set to true when experimenting during development
#define TRYING_THINGS false

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

static void printStructures(int argc, char* argv[], Bytecode* chunk, AstNode* ast) {
    bool showBytecode = cmdArgExists(argv, argc, "-asm");
    bool showTree = cmdArgExists(argv, argc, "-tree");
    
    if (showTree) {
        printf("==== AST ====\n\n");
        ast_print_tree(ast);
        printf("\n");
    }
    
    if (showBytecode) {
        printf("==== Bytecode ====\n\n");
        disassembler_do_bytecode(chunk);
        printf("\n");
    }
    
    if (showTree || showBytecode) {
    	printf("================\n");
    }
}

static void print_memory_diagnostic() {
    printf("======== Memory diagnostics ========");
    
    bool problem = false;

    size_t allocated_memory = get_allocated_memory();
    if (allocated_memory == 0) {
        DEBUG_IMPORTANT_PRINT("\n*******\nAll memory freed.\n*******\n");
    } else {
        DEBUG_IMPORTANT_PRINT("\n*******\nAllocated memory is %d, not 0!\n*******\n", allocated_memory);
        problem = true;
    }
    
    size_t num_allocations = get_allocations_count();
    if (num_allocations == 0) {
        DEBUG_IMPORTANT_PRINT("*******\nAll allocations freed.\n*******\n");
    } else {
        DEBUG_IMPORTANT_PRINT("*******\nNumber of allocations which have not been freed is %d, not 0!\n*******\n", num_allocations);
        problem = true;
    }
    
    if (problem) {
        memory_print_allocated_entries();
    }
    
    printf("\n*******\n");
    object_print_all_objects();
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

    memory_init();
    
//    char* source = readFile(argv[1]);
    char* source = NULL;
    size_t text_length = 0;

    if (io_read_file(argv[1], "Source file content", &source, &text_length) != IO_SUCCESS) {
    	return -1;
    }

//    if (source == NULL) {
//        return -1;
//    }
    
    DEBUG_PRINT("Starting CPlane!\n\n");

    // Must first init the VM, because some parts of the compiler depend on it
    vm_init();

    Bytecode chunk;
    bytecode_init(&chunk);
    AstNode* ast = parser_parse(source);
    compiler_compile(ast, &chunk);
    
	printStructures(argc, argv, &chunk, ast);
    ast_free_tree(ast);
//    free(source);
    deallocate(source, text_length, "Source file content");
    
    bool dryRun = checkCmdArg(argv, argc, 2, "-dry") || checkCmdArg(argv, argc, 3, "-dry") || checkCmdArg(argv, argc, 4, "-dry");
    if (!dryRun) {
    	InterpretResult result = vm_interpret(&chunk);
    }
    
    vm_free();
    
    #if DEBUG_IMPORTANT
    if (!dryRun) {
    	// Dry running does no GC, so some things from the compiler aren't cleaned... So no point in printing diagnostics.
    	// Not ideal, but leave this for now
    	print_memory_diagnostic();
    }
    #endif
    
    return 0; 
}
