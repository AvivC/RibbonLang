#ifndef plane_ast_h
#define plane_ast_h

#include "scanner.h"

typedef enum {
    // AST_NODE_VARIABLE,
    AST_NODE_CONSTANT,
    AST_NODE_BINARY
} AstNodeType;

typedef struct {
    AstNodeType type;
} AstNode;

typedef struct {
    AstNode base;
    double value;
} AstNodeConstant;

typedef struct {
    AstNode base;
    const char* name;
} AstNodeVariable;

typedef struct {
    AstNode base;
    TokenType operator;
    AstNode* leftOperand;
    AstNode* rightOperand;
} AstNodeBinary;

void printTree(AstNode* tree);

void freeTree(AstNode* node);

#endif