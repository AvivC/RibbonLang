#include <stdlib.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"
#include "object.h"
#include "pointerarray.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
} Parser;

static Parser parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY, // almost sure
    PREC_GROUPING
} Precedence;


typedef AstNode* (*PrefixFunction)(int expression_level);
typedef AstNode* (*InfixFunction)(AstNode* leftNode, int expression_level);

typedef struct {
    PrefixFunction prefix;
    InfixFunction infix;
    Precedence precedence;
} ParseRule;

static void error(const char* errorMessage) {
    fprintf(stderr, "On line %d: %s\n", parser.current.lineNumber, errorMessage);
    parser.hadError = true;
}

static bool check(ScannerTokenType type) {
    return parser.current.type == type;
}

static bool check_at_offset(ScannerTokenType type, int offset) {
	return peek_token_at_offset(offset).type == type;
}

static void advance() {
    parser.previous = parser.current;
    parser.current = scanToken();
    if (parser.current.type == TOKEN_ERROR) {
        error(parser.current.start);
    }
}

static void consume(ScannerTokenType type, const char* errorMessage) {
    if (check(type)) {
        advance();
        return;
    }
    error(errorMessage);
}

static bool match(ScannerTokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static bool matchNext(ScannerTokenType type) {
    if (peekNextToken().type == type) {
        advance();
        return true;
    }
    return false;
}

static bool match_at_offset(ScannerTokenType type, int offset) {
	if (peek_token_at_offset(offset).type == type) {
		advance();
		return true;
	}
	return false;
}

static void skipNewlines(void) {
	while (match(TOKEN_NEWLINE));
}

static AstNode* parse_expression(Precedence precedence, int expression_level);
static ParseRule get_rule(ScannerTokenType type);
static AstNode* statements();

static AstNode* binary(AstNode* leftNode, int expression_level) {
    ScannerTokenType operator = parser.previous.type;
    Precedence precedence = get_rule(operator).precedence;
    
    AstNode* rightNode = parse_expression(precedence + 1, expression_level + 1);
    
    AstNodeBinary* node = ALLOCATE_AST_NODE(AstNodeBinary, AST_NODE_BINARY);
    node->operator = operator;
    node->leftOperand = leftNode;
    node->rightOperand = rightNode;
    
    return (AstNode*) node;
}

static AstNode* dot(AstNode* leftNode, int expression_level) {
	// TODO: Very possibly not the best solution for attribute setting. Maybe refactor later.

	consume(TOKEN_IDENTIFIER, "Expected attribute name after '.'");
	const char* attr_name = parser.previous.start;
	int name_length = parser.previous.length;

	if (match(TOKEN_EQUAL)) { // TODO: Validate assignment is legal here
		if (expression_level != 0) {
			error("Attribute assignment illegal inside expression");
			return NULL;
		}
		// Set attribute
		AstNode* value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
		return (AstNode*) new_ast_node_attribute_assignment(leftNode, attr_name, name_length, value);
	} else {
		// Get attribute
		return (AstNode*) new_ast_node_attribute(leftNode, attr_name, name_length);
	}
}

static AstNode* returnStatement(void) {
	return (AstNode*) newAstNodeReturn(parse_expression(PREC_ASSIGNMENT, 0));
}

static AstNode* identifier(int expression_level) {
    AstNodeVariable* node = ALLOCATE_AST_NODE(AstNodeVariable, AST_NODE_VARIABLE);
    node->name = parser.previous.start;
    node->length = parser.previous.length;
    return (AstNode*) node;
}

static AstNode* number(int expression_level) {
    double number = strtod(parser.previous.start, NULL);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = MAKE_VALUE_NUMBER(number);
    return (AstNode*) node;
}

static AstNode* string(int expression_level) {
    const char* theString = parser.previous.start + 1;
    ObjectString* objString = copyString(theString, parser.previous.length - 2);
    Value value = MAKE_VALUE_OBJECT(objString);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = value;
    return (AstNode*) node;
}

static AstNode* and(AstNode* leftNode, int expression_level) {
	return (AstNode*) new_ast_node_and(leftNode, parse_expression(PREC_AND + 1, expression_level + 1));
}

static AstNode* or(AstNode* leftNode, int expression_level) {
	return (AstNode*) new_ast_node_or(leftNode, parse_expression(PREC_OR + 1, expression_level + 1));
}

static AstNode* function(int expression_level) {
	skipNewlines();

	PointerArray parameters;
	initPointerArray(&parameters);

	if (match(TOKEN_TAKES)) {
		do {
			consume(TOKEN_IDENTIFIER, "Expected parameter name.");
			char* parameter = copy_cstring(parser.previous.start, parser.previous.length, "ObjectFunction param cstring");
			writePointerArray(&parameters, parameter);
		} while (match(TOKEN_COMMA));

		consume(TOKEN_TO, "Expected 'to' at end of parameter list.");
	}

    AstNodeStatements* statementsNode = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of function.");

    AstNodeFunction* node = newAstNodeFunction(statementsNode, parameters);
    return (AstNode*) node;
}

static AstNode* call(AstNode* leftNode, int expression_level) {
	PointerArray arguments;
	initPointerArray(&arguments);

	while (!match(TOKEN_RIGHT_PAREN)) {
		do {
			AstNode* argument = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			writePointerArray(&arguments, argument);
		} while (match(TOKEN_COMMA));
	}

	return (AstNode*) newAstNodeCall(leftNode, arguments);
}

static AstNode* grouping(int expression_level) {
    AstNode* node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
    consume(TOKEN_RIGHT_PAREN, "Expected closing ')' after grouped expression.");
    return node;
}

static AstNode* unary(int expression_level) {
    AstNodeUnary* node = ALLOCATE_AST_NODE(AstNodeUnary, AST_NODE_UNARY);
    node->operand = parse_expression(PREC_UNARY, expression_level + 1); // so-called right associativity
    return (AstNode*) node;
}

static AstNode* boolean(int expression_level) {
	bool booleanValue;
	if (parser.previous.type == TOKEN_TRUE) {
		booleanValue = true;
	} else if (parser.previous.type == TOKEN_FALSE) {
		booleanValue = false;
	} else {
		FAIL("Illegal literal identified as boolean.");
	}
	return (AstNode*) newAstNodeConstant(MAKE_VALUE_BOOLEAN(booleanValue));
}

static void conditionedClause(AstNodeStatements** bodyOut, AstNode** conditionOut, int expression_level) {
	*conditionOut = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
	skipNewlines();
	consume(TOKEN_LEFT_BRACE, "Expected '{' to open block.");
	*bodyOut = (AstNodeStatements*) statements();
	consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of block.");
}

static AstNode* if_statement(void) {
	AstNodeStatements* body;
	AstNode* condition;
	conditionedClause(&body, &condition, 0);

	PointerArray elsifClauses;
	initPointerArray(&elsifClauses);

	while (match(TOKEN_ELSIF)) {
		AstNodeStatements* body;
		AstNode* condition;
		conditionedClause(&body, &condition, 0);
		writePointerArray(&elsifClauses, condition);
		writePointerArray(&elsifClauses, body);
	}

	AstNode* elseBody = NULL;
	if (match(TOKEN_ELSE)) {
		consume(TOKEN_LEFT_BRACE, "Expected '{' to open else-body.");
		elseBody = statements();
		consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of else-body.");
	}
	return (AstNode*) newAstNodeIf(condition, (AstNodeStatements*) body, elsifClauses, (AstNodeStatements*) elseBody);
}

static AstNode* while_statement(void) {
	AstNodeStatements* body;
	AstNode* condition;
	conditionedClause(&body, &condition, 0);

	return (AstNode*) newAstNodeWhile(condition, body);
}

static ParseRule rules[] = {
    {identifier, NULL, PREC_NONE},           // TOKEN_IDENTIFIER
    {number, NULL, PREC_NONE},         // TOKEN_NUMBER
    {string, NULL, PREC_NONE},         // TOKEN_STRING
    {NULL, binary, PREC_TERM},         // TOKEN_PLUS
    {unary, binary, PREC_TERM},         // TOKEN_MINUS
    {NULL, binary, PREC_FACTOR},       // TOKEN_STAR
    {NULL, binary, PREC_FACTOR},       // TOKEN_SLASH
    {NULL, NULL, PREC_NONE},     // TOKEN_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_BANG
    {NULL, binary, PREC_COMPARISON},     // TOKEN_LESS_THAN
    {NULL, binary, PREC_COMPARISON},     // TOKEN_GREATER_THAN
    {grouping, call, PREC_GROUPING},     // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_PAREN
    {function, NULL, PREC_NONE},     // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},     // TOKEN_NEWLINE
    {NULL, dot, PREC_GROUPING},     // TOKEN_DOT
    {NULL, binary, PREC_COMPARISON},     // TOKEN_EQUAL_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_BANG_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_GREATER_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},     // TOKEN_END
    {NULL, NULL, PREC_NONE},     // TOKEN_IF
    {NULL, NULL, PREC_NONE},     // TOKEN_ELSE
    {NULL, NULL, PREC_NONE},     // TOKEN_ELSIF
    {NULL, NULL, PREC_NONE},     // TOKEN_WHILE
    {NULL, NULL, PREC_NONE},     // TOKEN_FUNC
    {NULL, NULL, PREC_NONE},     // TOKEN_TAKES
    {NULL, NULL, PREC_NONE},     // TOKEN_TO
    {NULL, and, PREC_AND},     // TOKEN_AND
    {NULL, or, PREC_OR},     // TOKEN_OR
    {NULL, NULL, PREC_NONE},     // TOKEN_RETURN
    {boolean, NULL, PREC_NONE},     // TOKEN_TRUE
    {boolean, NULL, PREC_NONE},     // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},           // TOKEN_EOF
    {NULL, NULL, PREC_NONE}            // TOKEN_ERROR
};

static AstNode* parse_expression(Precedence precedence, int expression_level) {
    AstNode* node = NULL;
    
    advance(); // always assume the previous token is the "acting operator"
    ParseRule prefixRule = get_rule(parser.previous.type);
    if (prefixRule.prefix == NULL) {
    	printf("\n%d\n", parser.previous.type);
        error("Expecting a prefix operator."); // TODO: better message
        return NULL;
    }
    
    node = (AstNode*) prefixRule.prefix(expression_level);
    
    while (get_rule(parser.current.type).precedence >= precedence) { // TODO: also validate no NULL?
        advance();
        ParseRule infixRule = get_rule(parser.previous.type);
        node = (AstNode*) infixRule.infix(node, expression_level);
    }
    
    return node;
}

static AstNodeAssignment* assignment_statement(void) {
    const char* variableName = parser.previous.start;
    int variableLength = parser.previous.length;
    
    consume(TOKEN_EQUAL, "Expected '=' after variable name in assignment.");
    
    AstNode* value = parse_expression(PREC_ASSIGNMENT, 0);
    
    AstNodeAssignment* node = ALLOCATE_AST_NODE(AstNodeAssignment, AST_NODE_ASSIGNMENT);
    node->name = variableName;
    node->length = variableLength;
    node->value = value;
    
    return node;
}

static ParseRule get_rule(ScannerTokenType type) {
    return rules[type];
}

static AstNode* statements(void) {
    AstNodeStatements* statements_node = newAstNodeStatements();
    
    while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
        if (match(TOKEN_NEWLINE)) {
            continue;
        }
        
        AstNode* child_node = NULL;

        if (check(TOKEN_IDENTIFIER) && matchNext(TOKEN_EQUAL)) {
            child_node = (AstNode*) assignment_statement();
        } else if (match(TOKEN_RETURN)) {
        	child_node = (AstNode*) returnStatement();
        } else if (match(TOKEN_IF)) {
        	child_node = (AstNode*) if_statement();
        } else if (match(TOKEN_WHILE)) {
        	child_node = (AstNode*) while_statement();
    	} else {
    		AstNode* expression_or_attr_assignment = parse_expression(PREC_ASSIGNMENT, 0);
    		if (expression_or_attr_assignment->type == AST_NODE_ATTRIBUTE_ASSIGNMENT) {
    			child_node = expression_or_attr_assignment;
    		} else {
    			child_node = (AstNode*) newAstNodeExprStatement(expression_or_attr_assignment);
    		}
        }

        writePointerArray(&statements_node->statements, child_node);

		bool endOfBlock = check(TOKEN_EOF) || check(TOKEN_RIGHT_BRACE);
		if (!endOfBlock) {
            // Allow omitting the newline at the end of the function or program
            consume(TOKEN_NEWLINE, "Expected newline after statement.");
        }
        
        // Checking for errors on statement boundaries
        if (parser.hadError) {
            // TODO: Proper compiler error propagations and such
            fprintf(stderr, "Compiler exits with errors.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    return (AstNode*) statements_node;
}

AstNode* parse(const char* source) {
    initScanner(source);
    
    advance();
    parser.hadError = false;
    
    AstNode* node = statements();
    
    return node;
}
