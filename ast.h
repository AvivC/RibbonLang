#ifndef plane_ast_h
#define plane_ast_h

#include "scanner.h"
#include "value.h"
#include "pointerarray.h"

typedef enum {
    AST_NODE_CONSTANT,
    AST_NODE_BINARY,
    AST_NODE_UNARY,
    AST_NODE_VARIABLE,
    AST_NODE_ASSIGNMENT,
    AST_NODE_STATEMENTS,
    AST_NODE_FUNCTION,
    AST_NODE_CALL,
    AST_NODE_EXPR_STATEMENT,
    AST_NODE_RETURN
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
    const char* name;
    int length;
} AstNodeVariable;

typedef struct {
    AstNode base;
    AstNode* operand;
} AstNodeUnary;

typedef struct {
    AstNode base;
    ScannerTokenType operator;
    AstNode* leftOperand;
    AstNode* rightOperand;
} AstNodeBinary;

typedef struct {
    AstNode base;
    const char* name;
    int length;
    AstNode* value;
} AstNodeAssignment;

typedef struct {
    AstNode base;
    PointerArray statements;
} AstNodeStatements;

typedef struct {
    AstNode base;
    AstNodeStatements* statements;
    PointerArray parameters;
} AstNodeFunction;

typedef struct {
    AstNode base;
    AstNode* callTarget;
    PointerArray arguments;
} AstNodeCall;

typedef struct {
    AstNode base;
    AstNode* expression;
} AstNodeExprStatement;

typedef struct {
    AstNode base;
    AstNode* expression;
} AstNodeReturn;

void printTree(AstNode* tree);
void freeTree(AstNode* node);

AstNodeStatements* newAstNodeStatements();
AstNodeExprStatement* newAstNodeExprStatement();
AstNodeReturn* newAstNodeReturn(AstNode* expression);
AstNodeCall* newAstNodeCall(AstNode* expression, PointerArray arguments);
AstNodeFunction* newAstNodeFunction(AstNodeStatements* statements, PointerArray parameters);

#define ALLOCATE_AST_NODE(type, tag) (type*) allocateAstNode(tag, sizeof(type))

AstNode* allocateAstNode(AstNodeType type, size_t size);

#endif
