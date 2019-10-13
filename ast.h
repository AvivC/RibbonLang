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
	AST_NODE_KEY_ACCESS,
	AST_NODE_KEY_ASSIGNMENT,
	AST_NODE_TABLE,
	AST_NODE_IMPORT
} AstNodeType;

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
    AstNode* left_operand;
    AstNode* right_operand;
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
    AstNode* target;
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
    PointerArray elsif_clauses;
    AstNodeStatements* else_body;
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

typedef struct {
    AstNode base;
    AstNode* subject;
    AstNode* key;
    AstNode* value;
} AstNodeKeyAssignment;

typedef struct {
    AstNode base;
    const char* name; // Points to source code - do not free!
    int name_length;
} AstNodeImport;

typedef struct {
	AstNode* key;
	AstNode* value;
} AstNodesKeyValuePair;

DECLARE_DYNAMIC_ARRAY(AstNodesKeyValuePair, AstKeyValuePairArray, ast_key_value_pair_array)

typedef struct {
	AstNode base;
	AstKeyValuePairArray pairs;
} AstNodeTable;

void ast_print_tree(AstNode* tree);
void ast_free_tree(AstNode* node);

AstNodeConstant* ast_new_node_constant(Value value);
AstNodeStatements* ast_new_node_statements(void);
AstNodeExprStatement* ast_new_node_expr_statement(AstNode* expression);
AstNodeReturn* ast_new_node_return(AstNode* expression);
AstNodeCall* ast_new_node_call(AstNode* expression, PointerArray arguments);
AstNodeFunction* ast_new_node_function(AstNodeStatements* statements, ValueArray parameters);
AstNodeIf* ast_new_node_if(AstNode* condition, AstNodeStatements* body, PointerArray elsif_clauses, AstNodeStatements* else_body);
AstNodeWhile* ast_new_node_while(AstNode* condition, AstNodeStatements* body);
AstNodeAnd* ast_new_node_and(AstNode* left, AstNode* right);
AstNodeOr* ast_new_node_or(AstNode* left, AstNode* right);
AstNodeAttribute* ast_new_node_attribute(AstNode* object, const char* name, int length);
AstNodeAttributeAssignment* ast_new_node_attribute_assignment(AstNode* object, const char* name, int length, AstNode* value);
AstNodeString* ast_new_node_string(const char* string, int length);
AstNodeKeyAccess* ast_new_node_key_access(AstNode* key, AstNode* subject);
AstNodeKeyAssignment* ast_new_node_key_assignment(AstNode* key, AstNode* value, AstNode* subject);
AstNodeUnary* ast_new_node_unary(AstNode* expression);
AstNodeTable* ast_new_node_table(AstKeyValuePairArray pairs);
AstNodeImport* ast_new_node_import(const char* name, int name_length);

AstNodesKeyValuePair ast_new_key_value_pair(AstNode* key, AstNode* value);

#define ALLOCATE_AST_NODE(type, tag) (type*) ast_allocate_node(tag, sizeof(type))

AstNode* ast_allocate_node(AstNodeType type, size_t size);

#endif
