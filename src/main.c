#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>

#include "common.h"
#include "io.h"
#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "disassembler.h"
#include "value.h"
#include "ribbon_object.h"
#include "vm.h"
#include "memory.h"

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
        DEBUG_IMPORTANT_PRINT("\n*******\nAllocated memory is %" PRI_SIZET ", not 0!\n*******\n", allocated_memory);
        problem = true;
    }
    
    size_t num_allocations = get_allocations_count();
    if (num_allocations == 0) {
        DEBUG_IMPORTANT_PRINT("*******\nAll allocations freed.\n*******\n");
    } else {
        DEBUG_IMPORTANT_PRINT("*******\nNumber of allocations which have not been freed is %" PRI_SIZET ", not 0!\n*******\n", num_allocations);
        problem = true;
    }
    
    if (problem) {
        memory_print_allocated_entries();
    }

    printf("*******\n");
    object_print_all_objects();
    printf("*******\n");

    printf("======== End memory diagnostics ========\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 5) {
        fprintf(stdout, "Usage: ribbon <file> [[-asm] [-tree] [-dry]]");
        return -1;
    }

    memory_init();
    
    char* source = NULL;
    size_t text_length = 0;
    char* main_file_path = argv[1];
    char* abs_main_file_path;

    char* abs_path_alloc_string = "main module absolute path";

    if (PathIsRelativeA(main_file_path)) {
        char* working_dir = get_current_working_directory();
        abs_main_file_path = concat_multi_null_terminated_cstrings(
            3, (char*[]) {working_dir, "\\", main_file_path}, abs_path_alloc_string);
        deallocate(working_dir, strlen(working_dir) + 1, "working directory path");
    } else {
        /* Heap allocated because is later freed in vm_free, because it has to incase it was heap allocated when it's a relative path */
        abs_main_file_path = copy_null_terminated_cstring(main_file_path, abs_path_alloc_string);
    }

    if (io_read_text_file(abs_main_file_path, "Source file content", &source, &text_length) != IO_SUCCESS) {
        printf("Failed to open file.\n");
    	return -1;
    }

    DEBUG_PRINT("Starting Ribbon\n\n");

    /* Must first init the VM because some parts of the compiler depend on it */
    vm_init();

    Bytecode bytecode;
    bytecode_init(&bytecode);
    AstNode* ast = parser_parse(source, abs_main_file_path);
    compiler_compile(ast, &bytecode);
    
	printStructures(argc, argv, &bytecode, ast);
    ast_free_tree(ast);
    deallocate(source, text_length, "Source file content");
    
    bool dryRun = checkCmdArg(argv, argc, 2, "-dry") || checkCmdArg(argv, argc, 3, "-dry") || checkCmdArg(argv, argc, 4, "-dry");
    if (!dryRun) {
    	bool result = vm_interpret_program(&bytecode, abs_main_file_path);
    }
    
    vm_free();
    
    #if DEBUG_IMPORTANT
    if (!dryRun) {
    	// Dry running does no GC, so some objects from the compiler aren't cleaned... So no point in printing diagnostics.
        #if MEMORY_DIAGNOSTICS
    	print_memory_diagnostic();
        #endif
    }
    #endif
    
    return 0; 
}
