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
    fprintf(stdout, "On line %d: %s\n", parser.current.lineNumber, errorMessage);
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

static bool match_next(ScannerTokenType type) {
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

static ScannerTokenType mutate_operator_to_binary(ScannerTokenType operator) {
    switch (operator) {
        case TOKEN_PLUS_EQUALS: return TOKEN_PLUS;
        case TOKEN_MINUS_EQUALS: return TOKEN_MINUS;
        case TOKEN_STAR_EQUALS: return TOKEN_STAR;
        case TOKEN_SLASH_EQUALS: return TOKEN_SLASH;
        case TOKEN_MODULO_EQUALS: return TOKEN_MODULO;
        default: FAIL("Illegal operator in mutation parsing.");
    }
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

    int implicit_numeric_keys_count = 0;
	while (!match(TOKEN_RIGHT_SQUARE_BRACE)) {
		do {
            AstNode* key = NULL;
            AstNode* value = NULL;

            AstNode* expression = parse_expression(PREC_ASSIGNMENT, expression_level + 1);

			if (match(TOKEN_COLON)) {
                key = expression;
                value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
            } else {
                key = (AstNode*) ast_new_node_number(implicit_numeric_keys_count++);
                value = expression;
            }
            
			AstNodesKeyValuePair key_value = ast_new_key_value_pair(key, value);
			ast_key_value_pair_array_write(&pairs, &key_value);

            skip_newlines();
		} while (match(TOKEN_COMMA));
	}

	return (AstNode*) ast_new_node_table(pairs);
}


static AstNode* key_access(AstNode* left_node, int expression_level) {
	AstNode* key_node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
    skip_newlines();
	consume(TOKEN_RIGHT_SQUARE_BRACE, "Expected ']' after key.");

	if (match(TOKEN_EQUAL)) {
		// Set key

		if (expression_level != 0) {
			error("Key assignment illegal inside expression");
			return NULL;
		}

		AstNode* value_node = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
		return (AstNode*) ast_new_node_key_assignment(key_node, value_node, left_node);
	} else if (match(TOKEN_PLUS_EQUALS)
                || match(TOKEN_MINUS_EQUALS)
                || match(TOKEN_STAR_EQUALS)
                || match(TOKEN_SLASH_EQUALS)
                || match(TOKEN_MODULO_EQUALS)) { 
        // In place binary on key

        if (expression_level != 0) {
			error("Key in place binary operation illegal inside expression");
			return NULL;
		}

        ScannerTokenType in_place_operator = parser.previous.type;
		AstNode* value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);

        return (AstNode*) ast_new_node_in_place_key_binary(in_place_operator, left_node, key_node, value);
    } else {
		// Get key
		return (AstNode*) ast_new_node_key_access(key_node, left_node);
	}
}

static AstNode* dot(AstNode* left_node, int expression_level) {
	// TODO: Very possibly not the best solution for attribute setting. Maybe refactor later.

    skip_newlines();

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
	} else if (match(TOKEN_PLUS_EQUALS)
                || match(TOKEN_MINUS_EQUALS)
                || match(TOKEN_STAR_EQUALS)
                || match(TOKEN_SLASH_EQUALS)
                || match(TOKEN_MODULO_EQUALS)) { 
        if (expression_level != 0) {
			error("Attribute in place binary operation illegal inside expression");
			return NULL;
		}

        ScannerTokenType in_place_operator = parser.previous.type;

		// In place binary on attribute
		AstNode* value = parse_expression(PREC_ASSIGNMENT, expression_level + 1);

        return (AstNode*) ast_new_node_in_place_attribute_binary(in_place_operator, left_node, attr_name, name_length, value);
    } else {
		// Get attribute
		return (AstNode*) ast_new_node_attribute(left_node, attr_name, name_length);
	}
}

static AstNode* return_statement(void) {
    AstNode* expression;
    if (check(TOKEN_NEWLINE)) {
        expression = (AstNode*) ast_new_node_nil();
    } else {
        expression = (AstNode*) parse_expression(PREC_ASSIGNMENT, 1);
    }
    return (AstNode*) ast_new_node_return(expression);
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

	if (match(TOKEN_PIPE)) {
        while (!match(TOKEN_PIPE)) {
            if (check(TOKEN_EOF)) {
                error("File ends in the middle of parameter list.");
                break;
            }

            do {
                consume(TOKEN_IDENTIFIER, "Expected parameter name.");
                unsigned long hash = hash_string_bounded(parser.previous.start, parser.previous.length);
                Value param = MAKE_VALUE_RAW_STRING(parser.previous.start, parser.previous.length, hash);
                value_array_write(&parameters, &param);
            } while (match(TOKEN_COMMA));
        }
	}

    AstNodeStatements* statementsNode = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of function.");

    AstNodeFunction* node = ast_new_node_function(statementsNode, parameters);
    return (AstNode*) node;
}

static AstNode* call(AstNode* left_node, int expression_level) {
	PointerArray arguments;
	pointer_array_init(&arguments, "AstNodeCall arguments pointer array buffer");

	while (!check(TOKEN_RIGHT_PAREN) && !check(TOKEN_EOF)) {
		do {
            skip_newlines();
			AstNode* argument = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			pointer_array_write(&arguments, argument);
            skip_newlines();
		} while (match(TOKEN_COMMA));
	}

    consume(TOKEN_RIGHT_PAREN, "Expected ')' at end of argument list.");

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

static AstNode* nil(int expression_level) {
    return (AstNode*) ast_new_node_nil();
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

static AstNode* klass(int expression_level) {
    AstNode* superclass = NULL;
    if (match(TOKEN_COLON)) {
        superclass = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
    }
    skip_newlines();
    consume(TOKEN_LEFT_BRACE, "Expected '{' to open class definition.");
    AstNodeStatements* body = (AstNodeStatements*) statements();
    consume(TOKEN_RIGHT_BRACE, "Expected '}' to close class definition.");
    return (AstNode*) ast_new_node_class(body, superclass);
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

static AstNode* for_statement(void) {
    consume(TOKEN_IDENTIFIER, "Expected variable name after \"for\"");

    const char* variable_name = parser.previous.start;
    int variable_length = parser.previous.length;

    consume(TOKEN_IN, "Expected \"in\" after variable name in for statement.");

    AstNode* container = parse_expression(PREC_ASSIGNMENT, 1);

    skip_newlines();

    consume(TOKEN_LEFT_BRACE, "Expected '{' to open for loop body.");

    AstNodeStatements* body = (AstNodeStatements*) statements();

    consume(TOKEN_RIGHT_BRACE, "Expected '}' to close for loop body.");

    return (AstNode*) ast_new_node_for(variable_name, variable_length, container, body);
}

static AstNode* import_statement(void) {
	consume(TOKEN_IDENTIFIER, "Expected module name after 'import'.");
	const char* name = parser.previous.start;
	int name_length = parser.previous.length;
	return (AstNode*) ast_new_node_import(name, name_length);
}

static AstNode* global_statement(void) {
    consume(TOKEN_IDENTIFIER, "Expected variable name after \"global\".");
    return (AstNode*) ast_new_node_external(parser.previous.start, parser.previous.length);
}

static ParseRule rules[] = {
    {identifier, NULL, PREC_NONE},           // TOKEN_IDENTIFIER
    {number, NULL, PREC_NONE},         // TOKEN_NUMBER
    {string, NULL, PREC_NONE},         // TOKEN_STRING
    {NULL, binary, PREC_TERM},         // TOKEN_PLUS
    {unary, binary, PREC_TERM},         // TOKEN_MINUS
    {NULL, binary, PREC_FACTOR},       // TOKEN_STAR
    {NULL, binary, PREC_FACTOR},       // TOKEN_SLASH
    {NULL, binary, PREC_FACTOR},       // TOKEN_MODULO
    {NULL, NULL, PREC_NONE},       // TOKEN_PLUS_EQUALS
    {NULL, NULL, PREC_NONE},       // TOKEN_MINUS_EQUALS
    {NULL, NULL, PREC_NONE},       // TOKEN_STAR_EQUALS
    {NULL, NULL, PREC_NONE},       // TOKEN_SLASH_EQUALS
    {NULL, NULL, PREC_NONE},       // TOKEN_MODULO_EQUALS
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
    {NULL, NULL, PREC_NONE},     // TOKEN_PIPE
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
    {NULL, and, PREC_AND},     // TOKEN_AND
    {NULL, or, PREC_OR},     // TOKEN_OR
    {NULL, NULL, PREC_NONE},     // TOKEN_RETURN
    {boolean, NULL, PREC_NONE},     // TOKEN_TRUE
    {boolean, NULL, PREC_NONE},     // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},     // TOKEN_IMPORT
    {klass, NULL, PREC_NONE},     // TOKEN_CLASS
    {nil, NULL, PREC_NONE},     // TOKEN_NIL
    {NULL, NULL, PREC_NONE},     // TOKEN_FOR
    {NULL, NULL, PREC_NONE},     // TOKEN_IN
    {NULL, NULL, PREC_NONE},     // TOKEN_EXTERNAL
    {NULL, NULL, PREC_NONE},           // TOKEN_EOF
    {NULL, NULL, PREC_NONE}            // TOKEN_ERROR
};

static AstNode* parse_expression(Precedence precedence, int expression_level) {
    skip_newlines(); // Reconsider this?
    
    AstNode* node = NULL;
    
    advance(); // always assume the previous token is the "acting operator"
    ParseRule prefix_rule = get_rule(parser.previous.type);
    if (prefix_rule.prefix == NULL) {
        error("Expecting a prefix operator.");
        return NULL;
    }
    
    node = (AstNode*) prefix_rule.prefix(expression_level);
    
    while (get_rule(parser.current.type).precedence >= precedence) {
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
    AstNode* value = parse_expression(PREC_ASSIGNMENT, 1);

    return ast_new_node_assignment(variable_name, variable_length, value);
}


static AstNodeAssignment* mutation_statement(void) {
    const char* variable_name = parser.previous.start;
    int variable_length = parser.previous.length;

    advance();
    ScannerTokenType binary_operator = mutate_operator_to_binary(parser.previous.type);

    AstNode* value = parse_expression(PREC_ASSIGNMENT, 1);

    AstNodeVariable* get_variable_node = ast_new_node_variable(variable_name, variable_length);

    AstNodeBinary* binary = ast_new_node_binary(binary_operator, (AstNode*) get_variable_node, value);

    AstNodeAssignment* assignment = ast_new_node_assignment(variable_name, variable_length, (AstNode*) binary);

    return assignment;
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

        if (check(TOKEN_IDENTIFIER) && match_next(TOKEN_EQUAL)) {
            child_node = (AstNode*) assignment_statement();
        } else if (check(TOKEN_IDENTIFIER) 
                && (match_next(TOKEN_PLUS_EQUALS)
                || match_next(TOKEN_MINUS_EQUALS)
                || match_next(TOKEN_STAR_EQUALS)
                || match_next(TOKEN_SLASH_EQUALS)
                || match_next(TOKEN_MODULO_EQUALS))) {
            child_node = (AstNode*) mutation_statement();
        } else if (match(TOKEN_RETURN)) {
        	child_node = (AstNode*) return_statement();
        } else if (match(TOKEN_IF)) {
        	child_node = (AstNode*) if_statement();
        } else if (match(TOKEN_WHILE)) {
        	child_node = (AstNode*) while_statement();
    	} else if (match(TOKEN_FOR)) {
            child_node = (AstNode*) for_statement();
        } else if (match(TOKEN_IMPORT)) {
    		child_node = (AstNode*) import_statement();
        } else if (match(TOKEN_EXTERNAL)) {
            child_node = (AstNode*) global_statement();
    	} else {
    		AstNode* expr_or_attr_assignment_or_key_assignment = parse_expression(PREC_ASSIGNMENT, 0);
    		AstNodeType node_type = expr_or_attr_assignment_or_key_assignment->type;
    		if (node_type == AST_NODE_ATTRIBUTE_ASSIGNMENT 
                || node_type == AST_NODE_KEY_ASSIGNMENT
                || node_type == AST_NODE_IN_PLACE_ATTRIBUTE_BINARY
                || node_type == AST_NODE_IN_PLACE_KEY_BINARY) {
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
            fprintf(stdout, "Compiler exits with errors.\n");
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
