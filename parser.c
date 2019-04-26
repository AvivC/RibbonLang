#include <stdlib.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
} Parser;

static Parser parser;
static Chunk* compilingChunk;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_TERM,
    PREC_FACTOR,
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
    fprintf(stderr, "%s\n", errorMessage);
    parser.hadError = true;
}

static void advance() {
    parser.previous = parser.current;
    parser.current = scanToken();
    if (parser.current.type == TOKEN_ERROR) {
        error(parser.current.start);
    }
}

static void consume(TokenType type, const char* errorMessage) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    error(errorMessage);
}

static bool match(TokenType type) {
    if (parser.current.type == type) {
        advance();
        return true;
    }
    return false;
}

static AstNode* parsePrecedence(Precedence precedence);
static ParseRule getRule(TokenType type);

static AstNode* binary(AstNode* leftNode) {
    TokenType operator = parser.previous.type;
    Precedence precedence = getRule(operator).precedence;
    
    AstNode* rightNode = parsePrecedence(precedence + 1);
    
    AstNodeBinary* node = allocate(sizeof(AstNodeBinary));
    node->base.type = AST_NODE_BINARY;
    node->operator = operator;
    node->leftOperand = leftNode;
    node->rightOperand = rightNode;
    
    return (AstNode*) node;
}

static AstNode* number() {
    double number = strtod(parser.previous.start, NULL);
    AstNodeConstant* node = allocate(sizeof(AstNodeConstant));
    node->base.type = AST_NODE_CONSTANT;
    node->value = number;
    
    return (AstNode*) node;
}

static AstNode* grouping() {
    AstNode* node = parsePrecedence(PREC_ASSIGNMENT);
    consume(TOKEN_RIGHT_PAREN, "Expected closing ')' after grouped expression.");
    return node;
}

static ParseRule rules[] = {
    {NULL, NULL, PREC_NONE},           // TOKEN_IDENTIFIER
    {number, NULL, PREC_NONE},         // TOKEN_NUMBER
    {number, NULL, PREC_NONE},         // TOKEN_STRING
    {NULL, binary, PREC_TERM},         // TOKEN_PLUS
    {NULL, binary, PREC_TERM},         // TOKEN_MINUS
    {NULL, binary, PREC_FACTOR},       // TOKEN_STAR
    {NULL, binary, PREC_FACTOR},       // TOKEN_SLASH
    {NULL, NULL, PREC_NONE},     // TOKEN_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_BANG
    {NULL, NULL, PREC_NONE},     // TOKEN_LESS_THAN
    {NULL, NULL, PREC_NONE},     // TOKEN_GREATER_THAN
    {grouping, NULL, PREC_GROUPING},     // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},     // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},     // TOKEN_NEWLINE
    {NULL, NULL, PREC_NONE},     // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_GREATER_EQUAL
    {NULL, NULL, PREC_NONE},     // TOKEN_LESS_EQUAL
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

static ParseRule getRule(TokenType type) {
    // return pointer? nah
    return rules[type];
}

static AstNode* declarations() {
    // Currently only math statements
    return parsePrecedence(PREC_ASSIGNMENT);
}

AstNode* parse(const char* source) {
    initScanner(source);
    // compilingChunk = chunk;
    
    advance();
    parser.hadError = false;
    
    AstNode* node = declarations();
    
    return node;
    
    // for (;;) {
        // Token token = scanToken();
        // printf("Type: %d, text: '%.*s'\n", token.type, token.length, token.start);
        
        // if (token.type == TOKEN_EOF) {
            // break;
        // }
        // if (token.type == TOKEN_ERROR) {
            // fprintf(stderr, "%s\n", token.start);
            // break;
        // }
    // }
}