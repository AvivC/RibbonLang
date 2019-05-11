#ifndef plane_ast_h
#define plane_ast_h

#include "scanner.h"
#include "object.h"
#include "value.h"
#include "pointerarray.h"

typedef enum {
    AST_NODE_CONSTANT,
    AST_NODE_BINARY,
    AST_NODE_UNARY,
    AST_NODE_VARIABLE,
    AST_NODE_ASSIGNMENT,
    AST_NODE_STATEMENTS
} AstNodeType;

// for debugging purposes
extern const char* AST_NODE_TYPE_NAMES[];

typedef struct {
    AstNodeType type;
} AstNode;

typedef struct {
    AstNode base;
    Value value;
} AstNodeConstant;

typedef struct {
    AstNode base;
    ObjectString* name;
} AstNodeVariable;

typedef struct {
    AstNode base;
    AstNode* operand;
} AstNodeUnary;

typedef struct {
    AstNode base;
    TokenType operator;
    AstNode* leftOperand;
    AstNode* rightOperand;
} AstNodeBinary;

typedef struct {
    AstNode base;
    ObjectString* name;
    AstNode* value;
} AstNodeAssignment;

typedef struct {
    AstNode base;
    PointerArray statements;
} AstNodeStatements;

void printTree(AstNode* tree);
void freeTree(AstNode* node);

AstNodeStatements* newAstNodeStatements();

#define ALLOCATE_AST_NODE(type, tag) (type*) allocateAstNode(tag, sizeof(type))

AstNode* allocateAstNode(AstNodeType type, size_t size);

#endif