#include <ctype.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

typedef struct {
    const char* start;
    const char* current;
    // TODO: add line numbers
} Scanner;

static Scanner scanner;

static char current() {
    return *scanner.current;
}

static char advance() {
    char currentChar = current();
    scanner.current++;
    return currentChar;
}

static bool match(char c) {
    if (current() == c) {
        advance();
        return true;
    }
    
    return false;
}

static bool isEndOfCode(char c) {
    return c == '\0';
}

static bool isAlpha(char c) {
    return isalpha(c) || c == '_';
}

static bool isDigit(char c) {
    return isdigit(c);
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = scanner.current - scanner.start;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message); // assuming message is always a null-terminated string-literal
    return token;
}

static bool checkToken(const char* text) {
    int tokenLength = scanner.current - scanner.start;
    return (strlen(text) == tokenLength) && (strncmp(scanner.start, text, tokenLength) == 0);
}

static Token parseIdentifier() {
    while (isAlpha(current()) || isDigit(current())) {
        advance();
    }
    if (checkToken("end")) {
        return makeToken(TOKEN_END);
    } else if (checkToken("if")) {
        return makeToken(TOKEN_IF);
    } else if (checkToken("else")) {
        return makeToken(TOKEN_ELSE);
    } else if (checkToken("func")) {
        return makeToken(TOKEN_FUNC);
    } else if (checkToken("and")) {
        return makeToken(TOKEN_AND);
    } else if (checkToken("or")) {
        return makeToken(TOKEN_OR);
    }
    
    return makeToken(TOKEN_IDENTIFIER);
}

static Token parseNumber() {
    while (isDigit(current())) {
        advance();
    }
    
    if (current() == '.') {
        while(isDigit(current())) {
            advance();
        }
    }
    
    return makeToken(TOKEN_NUMBER);
}

static Token parseString() {
    while (current() != '"' && !isEndOfCode(current())) {
        advance();
    }
    
    if (isEndOfCode(current())) {
        return errorToken("Unterminated string.");
    }
    
    advance(); // Skip ending '"'
    
    printf("***%c***\n", current());
    return makeToken(TOKEN_STRING);
}

static void skipWhitespace() {
    // Newline is significant
    while (current() == ' ' || current() == '\t' || current() == '\v' || current() == '\r') {
        advance();
    }
}

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
}

Token peekNextToken() {
    // TODO: This approach is pretty awkward and can cause bugs later. Needs refactoring.
    
    const char* oldStart = scanner.start;
    const char* oldCurrent = scanner.current;
    Token token = scanToken();
    scanner.start = oldStart;
    scanner.current = oldCurrent;
    return token;
}

Token scanToken() {
    skipWhitespace();
    
    scanner.start = scanner.current;
    
    if (isEndOfCode(current())) {
        return makeToken(TOKEN_EOF);
    }
    
    char c = advance();
    
    if (isAlpha(c)) {
        return parseIdentifier();
    }
    
    if (isDigit(c)) {
        return parseNumber();
    }
    
    if (c == '"') {
        return parseString();
    }
    
    switch (c) {
        case '+': return makeToken(TOKEN_PLUS);
        case '-': return makeToken(TOKEN_MINUS);
        case '*': return makeToken(TOKEN_STAR);
        case '/': return makeToken(TOKEN_SLASH);
        case '=': return makeToken(TOKEN_EQUAL);
        case '!': return (match('=') ? makeToken(TOKEN_BANG_EQUAL) : makeToken(TOKEN_BANG));
        case '<': return (match('=') ? makeToken(TOKEN_LESS_EQUAL) : makeToken(TOKEN_LESS_THAN));
        case '>': return (match('=') ? makeToken(TOKEN_GREATER_EQUAL) : makeToken(TOKEN_GREATER_THAN));
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case ',': return makeToken(TOKEN_COMMA);
        case '\n': return makeToken(TOKEN_NEWLINE);
    }
    
    return errorToken("Unknown character.");
}
