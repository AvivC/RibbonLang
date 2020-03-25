#include <ctype.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

static Scanner scanner;

static char current() {
    return *scanner.current;
}

static char peek() {
    return *(scanner.current + 1);
}

static char advance() {
    char current_char = current();
    scanner.current++;
    if (current_char == '\n') {
        scanner.line++;
    }
    return current_char;
}

static bool match(char c) {
    if (current() == c) {
        advance();
        return true;
    }
    
    return false;
}

static bool is_end_of_code(char c) {
    return c == '\0';
}

static bool is_alpha(char c) {
    return isalpha(c) || c == '_';
}

static bool is_digit(char c) {
    return isdigit(c);
}

static Token make_token(ScannerTokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = scanner.current - scanner.start;
    token.lineNumber = scanner.line;

    DEBUG_SCANNER_PRINT("Token %d", type);
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message); // assuming message is always a null-terminated string-literal
    token.lineNumber = scanner.line;

    DEBUG_SCANNER_PRINT("Error token '%s'", message);

    return token;
}

static bool check_token(const char* text) {
    int token_length = scanner.current - scanner.start;
    return (strlen(text) == token_length) && (strncmp(scanner.start, text, token_length) == 0);
}

static Token parse_identifier() {
    while (is_alpha(current()) || is_digit(current())) {
        advance();
    }

    if (check_token("end")) {
        return make_token(TOKEN_END);
    } else if (check_token("if")) {
        return make_token(TOKEN_IF);
    } else if (check_token("else")) {
        return make_token(TOKEN_ELSE);
    } else if (check_token("elsif")) {
        return make_token(TOKEN_ELSIF);
    } else if (check_token("while")) {
    	return make_token(TOKEN_WHILE);
	} else if (check_token("and")) {
        return make_token(TOKEN_AND);
    } else if (check_token("or")) {
        return make_token(TOKEN_OR);
    } else if (check_token("return")) {
    	return make_token(TOKEN_RETURN);
    } else if (check_token("takes")) {
    	return make_token(TOKEN_TAKES);
    } else if (check_token("to")) {
    	return make_token(TOKEN_TO);
    } else if (check_token("true")) {
    	return make_token(TOKEN_TRUE);
    } else if (check_token("false")) {
    	return make_token(TOKEN_FALSE);
    } else if (check_token("not")) {
    	return make_token(TOKEN_NOT);
    } else if (check_token("import")) {
    	return make_token(TOKEN_IMPORT);
    } else if (check_token("class")) {
        return make_token(TOKEN_CLASS);
    }
    
    return make_token(TOKEN_IDENTIFIER);
}

static Token parse_number() {
    while (is_digit(current())) {
        advance();
    }
    
    if (current() == '.' && is_digit(peek())) {
        advance();
        while(is_digit(current())) {
            advance();
        }
    }
    
    return make_token(TOKEN_NUMBER);
}

static Token parse_string() {
    while (current() != '"' && !is_end_of_code(current())) {
        advance();
    }
    
    if (is_end_of_code(current())) {
        return error_token("Unterminated string.");
    }
    
    advance(); // Skip ending '"'
    
    return make_token(TOKEN_STRING);
}

static void skip_whitespace() {
    // Newline is significant
    while (current() == ' ' || current() == '\t' || current() == '\v' || current() == '\r') {
        advance();
    }
}

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1; // 1-based indexing for humans
}

Token scanner_peek_next_token() {
    return scanner_peek_token_at_offset(1);
}

Token scanner_peek_token_at_offset(int offset) {
	if (offset < 1) {
		FAIL("peek_token_at_offset called with offset < 1.");
	}

    // TODO: This approach is pretty awkward and can cause bugs later. Needs changing.

    const char* old_start = scanner.start;
    const char* old_current = scanner.current;

    Token token;
    for (int i = 0; i < offset; i++) {
    	token = scanner_next_token();
	}

    scanner.start = old_start;
    scanner.current = old_current;
    return token;
}

Token scanner_next_token() {
    skip_whitespace();
    while (current() == '#') {
    	do {
    		advance();
    	} while (current() != '\n' && !is_end_of_code(current()));
    }
    
    scanner.start = scanner.current;
    
    if (is_end_of_code(current())) {
        return make_token(TOKEN_EOF);
    }
    
    char c = advance();
    
    if (is_alpha(c)) {
        return parse_identifier();
    }
    
    if (is_digit(c)) {
        return parse_number();
    }
    
    if (c == '"') {
        return parse_string();
    }
    
    switch (c) {
        case '+': return make_token(TOKEN_PLUS);
        case '-': return make_token(TOKEN_MINUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '=': return (match('=') ? make_token(TOKEN_EQUAL_EQUAL) : make_token(TOKEN_EQUAL));
        case '<': return (match('=') ? make_token(TOKEN_LESS_EQUAL) : make_token(TOKEN_LESS_THAN));
        case '>': return (match('=') ? make_token(TOKEN_GREATER_EQUAL) : make_token(TOKEN_GREATER_THAN));
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '[': return make_token(TOKEN_LEFT_SQUARE_BRACE);
        case ']': return make_token(TOKEN_RIGHT_SQUARE_BRACE);
        case '\n': return make_token(TOKEN_NEWLINE);
        case ':': return make_token(TOKEN_COLON);
        case '!': {
			if (match('=')) {
				return make_token(TOKEN_BANG_EQUAL);
			}
			break;
		}
    }
    
    // TODO: More specific error reporting

    return error_token("Unknown character.");
}
