#include <stdlib.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"
#include "value_array.h"
#include "value.h"

typedef struct {
    Token current;
    Token previous;
    bool had_error;
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
typedef AstNode* (*InfixFunction)(AstNode* left_node, int expression_level);

typedef struct {
    PrefixFunction prefix;
    InfixFunction infix;
    Precedence precedence;
} ParseRule;

static void error(const char* errorMessage) {
    fprintf(stderr, "On line %d: %s\n", parser.current.lineNumber, errorMessage);
    parser.had_error = true;
}

static bool check(ScannerTokenType type) {
    return parser.current.type == type;
}

static bool check_at_offset(ScannerTokenType type, int offset) {
	return scanner_peek_token_at_offset(offset).type == type;
}

static void advance() {
    parser.previous = parser.current;
    parser.current = scanner_next_token();
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
    if (scanner_peek_next_token().type == type) {
        advance();
        return true;
    }
    return false;
}

static bool match_at_offset(ScannerTokenType type, int offset) {
	if (scanner_peek_token_at_offset(offset).type == type) {
		advance();
		return true;
	}
	return false;
}

static void skip_newlines(void) {
	while (match(TOKEN_NEWLINE));
}

static AstNode* parse_expression(Precedence precedence, int expression_level);
static ParseRule get_rule(ScannerTokenType type);
static AstNode* statements();

static AstNode* binary(AstNode* left_node, int expression_level) {
    ScannerTokenType operator = parser.previous.type;
    Precedence precedence = get_rule(operator).precedence;
    
    AstNode* right_node = parse_expression(precedence + 1, expression_level + 1);
    
    return (AstNode*) ast_new_node_binary(operator, left_node, right_node);
}

static AstNode* table(int expression_level) {
	AstKeyValuePairArray pairs;
	ast_key_value_pair_array_init(&pairs);

	while (!match(TOKEN_RIGHT_SQUARE_BRACE)) {
		do {
			AstNode* key = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			consume(TOKEN_COLON, "Expected ':' after key in table literal.");
			AstNode* value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			AstNodesKeyValuePair key_value = ast_new_key_value_pair(key, value);
			ast_key_value_pair_array_write(&pairs, &key_value);
		} while (match(TOKEN_COMMA));
	}

	return (AstNode*) ast_new_node_table(pairs);
}

static AstNode* key_access(AstNode* left_node, int expression_level) {
	AstNode* key_node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
	consume(TOKEN_RIGHT_SQUARE_BRACE, "Expected ']' after key.");

	if (match(TOKEN_EQUAL)) {
		// Set key

		if (expression_level != 0) {
			error("Key assignment illegal inside expression");
			return NULL;
		}

		AstNode* value_node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
		return (AstNode*) ast_new_node_key_assignment(key_node, value_node, left_node);
	} else {
		// Get key
		return (AstNode*) ast_new_node_key_access(key_node, left_node);
	}
}

static AstNode* dot(AstNode* left_node, int expression_level) {
	// TODO: Very possibly not the best solution for attribute setting. Maybe refactor later.

	consume(TOKEN_IDENTIFIER, "Expected attribute name after '.'");
	const char* attr_name = parser.previous.start;
	int name_length = parser.previous.length;

	if (match(TOKEN_EQUAL)) {
		if (expression_level != 0) {
			error("Attribute assignment illegal inside expression");
			return NULL;
		}
		// Set attribute
		AstNode* value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
		return (AstNode*) ast_new_node_attribute_assignment(left_node, attr_name, name_length, value);
	} else {
		// Get attribute
		return (AstNode*) ast_new_node_attribute(left_node, attr_name, name_length);
	}
}

static AstNode* return_statement(void) { // TODO: Add expression_level here?
	return (AstNode*) ast_new_node_return(parse_expression(PREC_ASSIGNMENT, 0));
}

static AstNode* identifier(int expression_level) {
	return (AstNode*) ast_new_node_variable(parser.previous.start, parser.previous.length);
}

static AstNode* number(int expression_level) {
    double number = strtod(parser.previous.start, NULL);
    return (AstNode*) ast_new_node_number(number);
}

static AstNode* string(int expression_level) {
    return (AstNode*) ast_new_node_string(parser.previous.start + 1, parser.previous.length - 2);
}

static AstNode* and(AstNode* left_node, int expression_level) {
	return (AstNode*) ast_new_node_and(left_node, parse_expression(PREC_AND + 1, expression_level + 1));
}

static AstNode* or(AstNode* left_node, int expression_level) {
	return (AstNode*) ast_new_node_or(left_node, parse_expression(PREC_OR + 1, expression_level + 1));
}

static AstNode* function(int expression_level) {
	skip_newlines();

	ValueArray parameters;
	value_array_init(&parameters);

	if (match(TOKEN_TAKES)) {
		do {
			consume(TOKEN_IDENTIFIER, "Expected parameter name.");
			Value param = MAKE_VALUE_RAW_STRING(parser.previous.start, parser.previous.length);
			value_array_write(&parameters, &param);
		} while (match(TOKEN_COMMA));

		consume(TOKEN_TO, "Expected 'to' at end of parameter list.");
	}

    AstNodeStatements* statementsNode = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of function.");

    AstNodeFunction* node = ast_new_node_function(statementsNode, parameters);
    return (AstNode*) node;
}

static AstNode* call(AstNode* left_node, int expression_level) {
	PointerArray arguments;
	pointer_array_init(&arguments, "AstNodeCall arguments pointer array buffer");

	while (!match(TOKEN_RIGHT_PAREN)) {
		do {
			AstNode* argument = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			pointer_array_write(&arguments, argument);
		} while (match(TOKEN_COMMA));
	}

	return (AstNode*) ast_new_node_call(left_node, arguments);
}

static AstNode* grouping(int expression_level) {
    AstNode* node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
    consume(TOKEN_RIGHT_PAREN, "Expected closing ')' after grouped expression.");
    return node;
}

static AstNode* unary(int expression_level) {
    return (AstNode*) ast_new_node_unary(parse_expression(PREC_UNARY, expression_level + 1)); // so-called right associativity
}

static AstNode* boolean(int expression_level) {
	bool boolean_value;
	if (parser.previous.type == TOKEN_TRUE) {
		boolean_value = true;
	} else if (parser.previous.type == TOKEN_FALSE) {
		boolean_value = false;
	} else {
		FAIL("Illegal literal identified as boolean.");
	}
	return (AstNode*) ast_new_node_constant(MAKE_VALUE_BOOLEAN(boolean_value));
}

static void conditioned_clause(AstNodeStatements** body_out, AstNode** condition_out, int expression_level) {
	*condition_out = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
	skip_newlines();
	consume(TOKEN_LEFT_BRACE, "Expected '{' to open block.");
	*body_out = (AstNodeStatements*) statements();
	consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of block.");
}

static AstNode* if_statement(void) {
	AstNodeStatements* body;
	AstNode* condition;
	conditioned_clause(&body, &condition, 0);

	PointerArray elsif_clauses;
	pointer_array_init(&elsif_clauses, "Elsif clauses pointer array");

	while (match(TOKEN_ELSIF)) {
		AstNodeStatements* body;
		AstNode* condition;
		conditioned_clause(&body, &condition, 0);
		pointer_array_write(&elsif_clauses, condition);
		pointer_array_write(&elsif_clauses, body);
	}

	AstNode* else_body = NULL;
	if (match(TOKEN_ELSE)) {
		consume(TOKEN_LEFT_BRACE, "Expected '{' to open else-body.");
		else_body = statements();
		consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of else-body.");
	}
	return (AstNode*) ast_new_node_if(condition, (AstNodeStatements*) body, elsif_clauses, (AstNodeStatements*) else_body);
}

static AstNode* while_statement(void) {
	AstNodeStatements* body;
	AstNode* condition;
	conditioned_clause(&body, &condition, 0);

	return (AstNode*) ast_new_node_while(condition, body);
}

static AstNode* import_statement(void) {
	consume(TOKEN_IDENTIFIER, "Expected module name after 'import'.");
	const char* name = parser.previous.start;
	int name_length = parser.previous.length;
	return (AstNode*) ast_new_node_import(name, name_length);
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
    {unary, NULL, PREC_NONE},     // TOKEN_NOT
    {NULL, binary, PREC_COMPARISON},     // TOKEN_LESS_THAN
    {NULL, binary, PREC_COMPARISON},     // TOKEN_GREATER_THAN
    {grouping, call, PREC_GROUPING},     // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_PAREN
    {function, NULL, PREC_NONE},     // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},     // TOKEN_NEWLINE
    {NULL, dot, PREC_GROUPING},     // TOKEN_DOT
    {table, key_access, PREC_GROUPING},     // TOKEN_LEFT_SQUARE_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_RIGHT_SQUARE_BRACE
    {NULL, NULL, PREC_NONE},     // TOKEN_COLON
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
    {NULL, NULL, PREC_NONE},     // TOKEN_IMPORT
    {NULL, NULL, PREC_NONE},           // TOKEN_EOF
    {NULL, NULL, PREC_NONE}            // TOKEN_ERROR
};

static AstNode* parse_expression(Precedence precedence, int expression_level) {
    AstNode* node = NULL;
    
    advance(); // always assume the previous token is the "acting operator"
    ParseRule prefix_rule = get_rule(parser.previous.type);
    if (prefix_rule.prefix == NULL) {
        error("Expecting a prefix operator."); // TODO: better message
        return NULL; // TODO: This makes the program crash...
    }
    
    node = (AstNode*) prefix_rule.prefix(expression_level);
    
    while (get_rule(parser.current.type).precedence >= precedence) { // TODO: also validate no NULL?
        advance();
        ParseRule infix_rule = get_rule(parser.previous.type);
        node = (AstNode*) infix_rule.infix(node, expression_level);
    }
    
    return node;
}

static AstNodeAssignment* assignment_statement(void) {
    const char* variable_name = parser.previous.start;
    int variable_length = parser.previous.length;

    consume(TOKEN_EQUAL, "Expected '=' after variable name in assignment.");
    AstNode* value = parse_expression(PREC_ASSIGNMENT, 0);

    return ast_new_node_assignment(variable_name, variable_length, value);
}

static ParseRule get_rule(ScannerTokenType type) {
    return rules[type];
}

static AstNode* statements(void) {
    AstNodeStatements* statements_node = ast_new_node_statements();
    
    while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
        if (match(TOKEN_NEWLINE)) {
            continue;
        }
        
        AstNode* child_node = NULL;

        if (check(TOKEN_IDENTIFIER) && matchNext(TOKEN_EQUAL)) {
            child_node = (AstNode*) assignment_statement();
        } else if (match(TOKEN_RETURN)) {
        	child_node = (AstNode*) return_statement();
        } else if (match(TOKEN_IF)) {
        	child_node = (AstNode*) if_statement();
        } else if (match(TOKEN_WHILE)) {
        	child_node = (AstNode*) while_statement();
    	} else if (match(TOKEN_IMPORT)) {
    		child_node = (AstNode*) import_statement();
    	} else {
    		AstNode* expr_or_attr_assignment_or_key_assignment = parse_expression(PREC_ASSIGNMENT, 0);
    		AstNodeType node_type = expr_or_attr_assignment_or_key_assignment->type;
    		if (node_type == AST_NODE_ATTRIBUTE_ASSIGNMENT || node_type == AST_NODE_KEY_ASSIGNMENT) {
    			child_node = expr_or_attr_assignment_or_key_assignment;
    		} else {
    			child_node = (AstNode*) ast_new_node_expr_statement(expr_or_attr_assignment_or_key_assignment);
    		}
        }

        pointer_array_write(&statements_node->statements, child_node);

		bool endOfBlock = check(TOKEN_EOF) || check(TOKEN_RIGHT_BRACE);
		if (!endOfBlock) {
            // Allow omitting the newline at the end of the function or program
            consume(TOKEN_NEWLINE, "Expected newline after statement.");
        }
        
        // Checking for errors on statement boundaries
        if (parser.had_error) {
            // TODO: Proper compiler error propagations and such
            fprintf(stderr, "Compiler exits with errors.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    return (AstNode*) statements_node;
}

AstNode* parser_parse(const char* source) {
    scanner_init(source);
    
    advance();
    parser.had_error = false;
    
    AstNode* node = statements();
    
    return node;
}
