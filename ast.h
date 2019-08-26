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
    AST_NODE_RETURN,
    AST_NODE_IF,
    AST_NODE_WHILE,
	AST_NODE_AND,
	AST_NODE_OR,
	AST_NODE_ATTRIBUTE,
	AST_NODE_ATTRIBUTE_ASSIGNMENT,
	AST_NODE_STRING,
	AST_NODE_KEY_ACCESS
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
    AstNode* object;
    const char* name;
    int length;
} AstNodeAttribute;

typedef struct {
    AstNode base;
    AstNode* object;
    const char* name;
    int length;
    AstNode* value;
} AstNodeAttributeAssignment;

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
    ValueArray parameters;
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

typedef struct {
    AstNode base;
    AstNode* condition;
    AstNodeStatements* body;
    PointerArray elsifClauses;
    AstNodeStatements* elseBody;
} AstNodeIf;

typedef struct {
    AstNode base;
    AstNode* condition;
    AstNodeStatements* body;
} AstNodeWhile;

typedef struct {
	AstNode base;
	AstNode* left;
	AstNode* right;
} AstNodeAnd;

typedef struct {
	AstNode base;
	AstNode* left;
	AstNode* right;
} AstNodeOr;

typedef struct {
	AstNode base;
	const char* string;
	int length;
} AstNodeString;

typedef struct {
	AstNode base;
	AstNode* key;
	AstNode* subject;
} AstNodeKeyAccess;

void printTree(AstNode* tree);
void freeTree(AstNode* node);

AstNodeConstant* newAstNodeConstant(Value value);
AstNodeStatements* newAstNodeStatements(void);
AstNodeExprStatement* new_ast_node_expr_statement(AstNode* expression);
AstNodeReturn* newAstNodeReturn(AstNode* expression);
AstNodeCall* newAstNodeCall(AstNode* expression, PointerArray arguments);
AstNodeFunction* newAstNodeFunction(AstNodeStatements* statements, ValueArray parameters);
AstNodeIf* new_ast_node_ff(AstNode* condition, AstNodeStatements* body, PointerArray elsifClauses, AstNodeStatements* elseBody);
AstNodeWhile* new_ast_node_while(AstNode* condition, AstNodeStatements* body);
AstNodeAnd* new_ast_node_and(AstNode* left, AstNode* right);
AstNodeOr* new_ast_node_or(AstNode* left, AstNode* right);
AstNodeAttribute* new_ast_node_attribute(AstNode* object, const char* name, int length);
AstNodeAttributeAssignment* new_ast_node_attribute_assignment(AstNode* object, const char* name, int length, AstNode* value);
AstNodeString* new_ast_node_string(const char* string, int length);
AstNodeKeyAccess* new_ast_node_key_access(AstNode* key, AstNode* subject);

#define ALLOCATE_AST_NODE(type, tag) (type*) allocateAstNode(tag, sizeof(type))

AstNode* allocateAstNode(AstNodeType type, size_t size);

#endif
