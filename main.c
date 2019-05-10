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

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    
    if (file == NULL) {
        fprintf(stderr, "Couldn't open file.\n");
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

int main(int argc, char* argv[]) {
    
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: plane <file> [-asm] [-tree]");
        return -1;
    }
    const char* filePath = argv[1];
    char* source = readFile(filePath);
    
    if (source == NULL) {
        return -1;
    }
    
    printf("Starting CPlane!\n\n");
    
    Chunk chunk;
    initChunk(&chunk);
    AstNode* ast = parse(source);
    compile(ast, &chunk);
    
    if (argc > 2) {
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
            disassembleChunk(&chunk);
            printf("\n");
        }
    }
    
    printf("==== Executing ====\n\n");
    initVM(&chunk);
    InterpretResult result = interpret();
    
    printf("\n");
    
    freeTree(ast);
    free(source);
    
    return 0; 
}
