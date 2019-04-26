#include "compiler.h"
#include "parser.h"
#include "chunk.h"
#include "ast.h"

static void compileTree(AstNode* node, Chunk* chunk) {
    AstNodeType nodeType = node->type;
    
    switch (nodeType) {
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            TokenType operator = nodeBinary->operator;
            AstNode* leftOperand = nodeBinary->leftOperand;
            AstNode* rightOperand = nodeBinary->rightOperand;
            
            compileTree(leftOperand, chunk);
            compileTree(rightOperand, chunk);
            
            switch (operator) {
                case TOKEN_PLUS: writeChunk(chunk, OP_ADD); break;
                case TOKEN_MINUS: writeChunk(chunk, OP_SUBTRACT); break;
                case TOKEN_STAR: writeChunk(chunk, OP_MULTIPLY); break;
                case TOKEN_SLASH: writeChunk(chunk, OP_DIVIDE); break;
                default: fprintf(stderr, "Weird operator type!\n"); break;
            }
            
            break;
        }
        
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            
            double number = nodeConstant->value;
            Value value;
            value.type = VALUE_NUMBER;
            value.as.number = number;
            
            int constantIndex = addConstant(chunk, value);
            
            writeChunk(chunk, OP_CONSTANT);
            writeChunk(chunk, (uint8_t) constantIndex);
            
            break;
        }
    }
    
}

void compileSource(const char* source, Chunk* chunk) {
    AstNode* node = parse(source);
    compileTree(node, chunk);
    writeChunk(chunk, OP_RETURN); // temporary
}
