#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "vm.h"

int main(int argc, char* argv[]) {
    printf("Starting CPlane!\n");
    
    Chunk chunk;
    initChunk(&chunk);
    
    // const char* source = "3 * 4 + 2 / 2 /4 / 1000 * 3 + 1 + 2 + 3023 - 3";
    const char* source = " 2 + (3 + 10) /2     ";
    // AstNode* node = parse(source);
    // printTree(node);
    
    compileSource(source, &chunk);
    
    initVM(&chunk);
    InterpretResult result = interpret();
    
    // disassembleChunk(&chunk);
    
    // freeTree(node);
    
    return 0; 
}
