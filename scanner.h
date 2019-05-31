#ifndef plane_scanner_h
#define plane_scanner_h

typedef enum {
    // Token types
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    
    // One-character symbols
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_EQUAL, TOKEN_BANG, TOKEN_LESS_THAN, TOKEN_GREATER_THAN,
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_NEWLINE,
    
    // Two-character symbols
    TOKEN_BANG_EQUAL, TOKEN_GREATER_EQUAL, TOKEN_LESS_EQUAL,
    
    // Alphabetic symbols
    TOKEN_PRINT, TOKEN_END, TOKEN_IF, TOKEN_ELSE, TOKEN_FUNC, TOKEN_AND, TOKEN_OR,
    
    // Special tokens
    TOKEN_EOF, TOKEN_ERROR
} ScannerTokenType;

typedef struct {
    ScannerTokenType type;
    const char* start;
    int length;
    int lineNumber;
} Token;

void initScanner(const char* source);
Token peekNextToken();
Token scanToken();

#endif
