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


typedef AstNode* (*PrefixFunction)(void);
typedef AstNode* (*InfixFunction)(AstNode* leftNode);

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

static void skipNewlines(void) {
	while (match(TOKEN_NEWLINE));
}

static AstNode* parsePrecedence(Precedence precedence);
static ParseRule getRule(ScannerTokenType type);
static AstNode* statements();

static AstNode* binary(AstNode* leftNode) {
    ScannerTokenType operator = parser.previous.type;
    Precedence precedence = getRule(operator).precedence;
    
    AstNode* rightNode = parsePrecedence(precedence + 1);
    
    AstNodeBinary* node = ALLOCATE_AST_NODE(AstNodeBinary, AST_NODE_BINARY);
    node->operator = operator;
    node->leftOperand = leftNode;
    node->rightOperand = rightNode;
    
    return (AstNode*) node;
}

static AstNode* returnStatement(void) {
	return (AstNode*) newAstNodeReturn(parsePrecedence(PREC_ASSIGNMENT));
}

static AstNode* identifier(void) {
    AstNodeVariable* node = ALLOCATE_AST_NODE(AstNodeVariable, AST_NODE_VARIABLE);
    node->name = parser.previous.start;
    node->length = parser.previous.length;
    return (AstNode*) node;
}

static AstNode* number(void) {
    double number = strtod(parser.previous.start, NULL);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = MAKE_VALUE_NUMBER(number);
    return (AstNode*) node;
}

static AstNode* string(void) {
    const char* theString = parser.previous.start + 1;
    ObjectString* objString = copyString(theString, parser.previous.length - 2);
    Value value = MAKE_VALUE_OBJECT(objString);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = value;
    return (AstNode*) node;
}

static AstNode* function(void) {
	skipNewlines();

	PointerArray parameters;
	initPointerArray(&parameters);

	if (match(TOKEN_TAKES)) {
		do {
			consume(TOKEN_IDENTIFIER, "Expected parameter name.");
			writePointerArray(&parameters, copyString(parser.previous.start, parser.previous.length));
		} while (match(TOKEN_COMMA));

		consume(TOKEN_TO, "Expected 'to' at end of parameter list.");
	}

    AstNodeStatements* statementsNode = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of function.");

    AstNodeFunction* node = newAstNodeFunction(statementsNode, parameters);
    return (AstNode*) node;
}

static AstNode* call(AstNode* leftNode) {
	PointerArray arguments;
	initPointerArray(&arguments);

	while (!match(TOKEN_RIGHT_PAREN)) {
		do {
			AstNode* argument = parsePrecedence(PREC_ASSIGNMENT);
			writePointerArray(&arguments, argument);
		} while (match(TOKEN_COMMA));
	}

	return (AstNode*) newAstNodeCall(leftNode, arguments);
}

static AstNode* grouping(void) {
    AstNode* node = parsePrecedence(PREC_ASSIGNMENT);
    consume(TOKEN_RIGHT_PAREN, "Expected closing ')' after grouped expression.");
    return node;
}

static AstNode* unary(void) {
    AstNodeUnary* node = ALLOCATE_AST_NODE(AstNodeUnary, AST_NODE_UNARY);
    node->operand = parsePrecedence(PREC_UNARY); // so-called right associativity
    return (AstNode*) node;
}

static AstNode* boolean(void) {
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

static AstNode* ifStatement(void) {
	AstNode* condition = parsePrecedence(PREC_ASSIGNMENT);
	skipNewlines();
	consume(TOKEN_LEFT_BRACE, "Expected '{' to open if-body.");
	AstNode* body = statements();
	consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of if-body.");
	AstNode* elseBody = NULL;
	if (match(TOKEN_ELSE)) {
		consume(TOKEN_LEFT_BRACE, "Expected '{' to open else-body.");
		elseBody = statements();
		consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of else-body.");
	}
	return (AstNode*) newAstNodeIf(condition, (AstNodeStatements*) body, (AstNodeStatements*) elseBody);
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
    {NULL, binary, PREC_COMPARISON},     // TOKEN_EQUAL_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_BANG_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_GREATER_EQUAL
    {NULL, binary, PREC_COMPARISON},     // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},     // TOKEN_END
    {NULL, NULL, PREC_NONE},     // TOKEN_IF
    {NULL, NULL, PREC_NONE},     // TOKEN_ELSE
    {NULL, NULL, PREC_NONE},     // TOKEN_FUNC
    {NULL, NULL, PREC_NONE},     // TOKEN_TAKES
    {NULL, NULL, PREC_NONE},     // TOKEN_TO
    {NULL, NULL, PREC_NONE},     // TOKEN_AND
    {NULL, NULL, PREC_NONE},     // TOKEN_OR
    {NULL, NULL, PREC_NONE},     // TOKEN_RETURN
    {boolean, NULL, PREC_NONE},     // TOKEN_TRUE
    {boolean, NULL, PREC_NONE},     // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},           // TOKEN_EOF
    {NULL, NULL, PREC_NONE}            // TOKEN_ERROR
};

static AstNode* parsePrecedence(Precedence precedence) {
    AstNode* node = NULL;
    
    advance(); // always assume the previous token is the "acting operator"
    ParseRule prefixRule = getRule(parser.previous.type);
    if (prefixRule.prefix == NULL) {
    	printf("\n%d\n", parser.previous.type);
        error("Expecting a prefix operator."); // TODO: better message
        return NULL;
    }
    
    node = (AstNode*) prefixRule.prefix();
    
    while (getRule(parser.current.type).precedence >= precedence) { // TODO: also validate no NULL?
        advance();
        ParseRule infixRule = getRule(parser.previous.type);
        node = (AstNode*) infixRule.infix(node);
    }
    
    return node;
}

static AstNodeAssignment* assignmentStatement() {
    const char* variableName = parser.previous.start;
    int variableLength = parser.previous.length;
    
    consume(TOKEN_EQUAL, "Expected '=' after variable name in assignment.");
    
    AstNode* value = parsePrecedence(PREC_ASSIGNMENT);
    
    AstNodeAssignment* node = ALLOCATE_AST_NODE(AstNodeAssignment, AST_NODE_ASSIGNMENT);
    node->name = variableName;
    node->length = variableLength;
    node->value = value;
    
    return node;
}

static ParseRule getRule(ScannerTokenType type) {
    return rules[type];
}

static AstNodeExprStatement* expressionStatement() {
	return newAstNodeExprStatement(parsePrecedence(PREC_ASSIGNMENT));
}

static AstNode* statements() {
    AstNodeStatements* statementsNode = newAstNodeStatements();
    
    while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
        if (match(TOKEN_NEWLINE)) {
            continue;
        }
        
        AstNode* childNode = NULL;

        if (check(TOKEN_IDENTIFIER) && matchNext(TOKEN_EQUAL)) {
            childNode = (AstNode*) assignmentStatement();
        } else if (match(TOKEN_RETURN)) {
        	childNode = (AstNode*) returnStatement();
        } else if (match(TOKEN_IF)) {
        	childNode = (AstNode*) ifStatement();
        } else {
			childNode = (AstNode*) expressionStatement();
        }

        writePointerArray(&statementsNode->statements, childNode);

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
    
    return (AstNode*) statementsNode;
}

AstNode* parse(const char* source) {
    initScanner(source);
    
    advance();
    parser.hadError = false;
    
    AstNode* node = statements();
    
    return node;
}
