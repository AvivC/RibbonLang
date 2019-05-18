#include <stdlib.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"
#include "object.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
} Parser;

static Parser parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY, // almost sure
    PREC_GROUPING
} Precedence;


typedef AstNode* (*PrefixFunction)();
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

static AstNode* identifier() {
    AstNodeVariable* node = ALLOCATE_AST_NODE(AstNodeVariable, AST_NODE_VARIABLE);
    node->name = parser.previous.start;
    node->length = parser.previous.length;
    return (AstNode*) node;
}

static AstNode* number() {
    double number = strtod(parser.previous.start, NULL);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = MAKE_VALUE_NUMBER(number);
    return (AstNode*) node;
}

static AstNode* string() {
    const char* theString = parser.previous.start + 1;
    ObjectString* objString = copyString(theString, parser.previous.length - 2);
    Value value = MAKE_VALUE_OBJECT(objString);
    AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
    node->value = value;
    return (AstNode*) node;
}

static AstNode* function() {
    AstNodeStatements* statementsNode = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of function.");
    AstNodeFunction* node = ALLOCATE_AST_NODE(AstNodeFunction, AST_NODE_FUNCTION);
    node->statements = statementsNode;
    return (AstNode*) node;
}

static AstNode* call(AstNode* leftNode) {
    // TODO: add arguments and such
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after function call.");
    AstNodeCall* node = ALLOCATE_AST_NODE(AstNodeCall, AST_NODE_CALL);
    node->callTarget = leftNode;
    return (AstNode*) node;
}

static AstNode* grouping() {
    AstNode* node = parsePrecedence(PREC_ASSIGNMENT);
    consume(TOKEN_RIGHT_PAREN, "Expected closing ')' after grouped expression.");
    return node;
}

static AstNode* unary() {
    AstNodeUnary* node = ALLOCATE_AST_NODE(AstNodeUnary, AST_NODE_UNARY);
    node->operand = parsePrecedence(PREC_UNARY); // so-called right associativity
    return (AstNode*) node;
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
    {NULL, NULL, PREC_NONE},     // TOKEN_LESS_THAN
    {NULL, NULL, PREC_NONE},     // TOKEN_GREATER_THAN
    {grouping, call, PREC_GROUPING},     // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_PAREN
    {function, NULL, PREC_NONE},     // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},     // TOKEN_NEWLINE
    {NULL, NULL, PREC_NONE},     // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_GREATER_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},     // TOKEN_END
    {NULL, NULL, PREC_NONE},     // TOKEN_IF
    {NULL, NULL, PREC_NONE},     // TOKEN_ELSE
    {NULL, NULL, PREC_NONE},     // TOKEN_FUNC
    {NULL, NULL, PREC_NONE},     // TOKEN_AND
    {NULL, NULL, PREC_NONE},     // TOKEN_OR
    {NULL, NULL, PREC_NONE},           // TOKEN_EOF
    {NULL, NULL, PREC_NONE}            // TOKEN_ERROR
};

static AstNode* parsePrecedence(Precedence precedence) {
    AstNode* node = NULL;
    
    advance(); // always assume the previous token is the "acting operator"
    ParseRule prefixRule = getRule(parser.previous.type);
    if (prefixRule.prefix == NULL) {
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

static AstNode* assignmentStatement() {
    const char* variableName = parser.previous.start;
    int variableLength = parser.previous.length;
    
    consume(TOKEN_EQUAL, "Expected '=' after variable name in assignment.");
    
    AstNode* value = parsePrecedence(PREC_ASSIGNMENT);
    
    AstNodeAssignment* node = ALLOCATE_AST_NODE(AstNodeAssignment, AST_NODE_ASSIGNMENT);
    node->name = variableName;
    node->length = variableLength;
    node->value = value;
    
    return (AstNode*) node;
}

static ParseRule getRule(ScannerTokenType type) {
    // return pointer? nah
    return rules[type];
}

static AstNode* statements() {
    AstNodeStatements* statementsNode = newAstNodeStatements();
    
    while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
        if (match(TOKEN_NEWLINE)) {
            continue;
        }
        
        if (check(TOKEN_IDENTIFIER) && matchNext(TOKEN_EQUAL)) {
            AstNode* assignmentNode = assignmentStatement();
            writePointerArray(&statementsNode->statements, assignmentNode);
        } else {
            AstNode* expressionNode = parsePrecedence(PREC_ASSIGNMENT);
            writePointerArray(&statementsNode->statements, expressionNode);
        }
        
        if (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
            // Allowing omitting the newline at the end of the function or program
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