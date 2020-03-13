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
    
    if (problem) { // Uncomment
        memory_print_allocated_entries();
    }

    // memory_print_allocated_entries(); // Remove
    
    printf("*******\n");
    object_print_all_objects();
    printf("*******\n");

    printf("======== End memory diagnostics ========\n");
}

int main(int argc, char* argv[]) {
	// To make experimenting while working easy
	if (TRYING_THINGS) {
		printf("--- Experimentation mode. ---\n");

        memory_init();

        Table table;
        table_init_memory_infrastructure(&table);

        Value v1 = MAKE_VALUE_NUMBER(1);
        Value v2 = MAKE_VALUE_NUMBER(2);
        Value v3 = MAKE_VALUE_NUMBER(3);
        Value v4 = MAKE_VALUE_NUMBER(4);
        Value v5 = MAKE_VALUE_NUMBER(5);
        Value v6 = MAKE_VALUE_NUMBER(6);
        Value v7 = MAKE_VALUE_NUMBER(7);
        Value v8 = MAKE_VALUE_NUMBER(8);
        Value v9 = MAKE_VALUE_NUMBER(9);
        Value v10 = MAKE_VALUE_NUMBER(10);

        // Value k1 = MAKE_VALUE_NUMBER(100);
        // Value k2 = MAKE_VALUE_NUMBER(200);
        // Value k3 = MAKE_VALUE_NUMBER(300);
        // Value k4 = MAKE_VALUE_NUMBER(400);
        // Value k5 = MAKE_VALUE_NUMBER(500);
        // Value k6 = MAKE_VALUE_NUMBER(600);
        // Value k7 = MAKE_VALUE_NUMBER(700);
        // Value k8 = MAKE_VALUE_NUMBER(800);
        // Value k9 = MAKE_VALUE_NUMBER(900);
        // Value k10 = MAKE_VALUE_NUMBER(1000);

        Value k1 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(97));
        Value k2 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(4));
        Value k3 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(22));
        Value k4 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(25));
        Value k5 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(15));
        Value k6 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(15));
        Value k7 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(207));
        Value k8 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(15));
        Value k9 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(642));
        Value k10 = MAKE_VALUE_ADDRESS((uintptr_t) malloc(4));


        table_set(&table, k1, v1); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k2, v2); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k3, v3); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k4, v4); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k5, v5); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k6, v6); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k7, v7); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k8, v8); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k9, v9); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k10, v10); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");

        Value k11 = MAKE_VALUE_NUMBER(1100);
        Value v11 = MAKE_VALUE_NUMBER(11);
        Value k12 = MAKE_VALUE_NUMBER(1200);
        Value v12 = MAKE_VALUE_NUMBER(12);
        Value k13 = MAKE_VALUE_NUMBER(1300);
        Value v13 = MAKE_VALUE_NUMBER(13);
        table_set(&table, k11, v11); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k12, v12); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, k13, v13); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");

        table_set(&table, k6, MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");

        table_set(&table, MAKE_VALUE_NUMBER(-1), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-2), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-3), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_delete(&table, k4);
        table_delete(&table, k2);
        table_delete(&table, k13);
        table_delete(&table, k10);
        table_set(&table, MAKE_VALUE_NUMBER(-4), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-5), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-6), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-7), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-8), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-9), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-10), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");
        table_set(&table, MAKE_VALUE_NUMBER(-11), MAKE_VALUE_NUMBER(-1)); table_print_debug_as_buckets(&table, true); printf("\n---------------------------\n");

        table_delete(&table, k8);

        // table_print_debug(&table);


        PointerArray entries = table_iterate(&table, "stuff");

        for (size_t i = 0; i < entries.count; i++) {
            Node* entry = entries.values[i];
            printf("\n %d: %p\n", i, entry);

            // uintptr_t address = entry->key.as.address;
            void* address = (void*) entry->key.as.address;
            Allocation allocation = entry->value.as.allocation;        
        }


        table_print_debug_as_buckets(&table, false);
        printf("\n");

        pointer_array_free(&entries);

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

    allocate(1024, "Dummy"); // Just for testing, remove later

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
