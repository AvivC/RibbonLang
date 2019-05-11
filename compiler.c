#include "compiler.h"
#include "parser.h"
#include "chunk.h"
#include "ast.h"
#include "value.h"

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
            
            int constantIndex = addConstant(chunk, nodeConstant->value);
            
            writeChunk(chunk, OP_CONSTANT);
            writeChunk(chunk, (uint8_t) constantIndex);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            compileTree(nodeUnary->operand, chunk);
            writeChunk(chunk, OP_NEGATE);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            
            Value nameValue = MAKE_VALUE_OBJECT(nodeVariable->name);
            int constantIndex = addConstant(chunk, nameValue);
            
            writeChunk(chunk, OP_LOAD_VARIABLE);
            writeChunk(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            
            compileTree(nodeAssignment->value, chunk);
            
            Value nameValue = MAKE_VALUE_OBJECT(nodeAssignment->name);
            int constantIndex = addConstant(chunk, nameValue);
            
            writeChunk(chunk, OP_SET_VARIABLE);
            writeChunk(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                DEBUG_PRINT("Compiling statement: index %d out of %d\n", i, nodeStatements->statements.count);
                compileTree((AstNode*) nodeStatements->statements.values[i], chunk);
            }
            
            break;
        }
    }
}

void compile(AstNode* node, Chunk* chunk) {
    compileTree(node, chunk);
    writeChunk(chunk, OP_RETURN); // temporary
}
