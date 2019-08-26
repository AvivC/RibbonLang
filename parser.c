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
typedef AstNode* (*InfixFunction)(AstNode* leftNode, int expression_level);

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

static void skip_newlines(void) {
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

static AstNode* return_statement(void) {
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
    return (AstNode*) new_ast_node_string(parser.previous.start + 1, parser.previous.length - 2);
}

static AstNode* and(AstNode* leftNode, int expression_level) {
	return (AstNode*) new_ast_node_and(leftNode, parse_expression(PREC_AND + 1, expression_level + 1));
}

static AstNode* or(AstNode* leftNode, int expression_level) {
	return (AstNode*) new_ast_node_or(leftNode, parse_expression(PREC_OR + 1, expression_level + 1));
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

    AstNodeFunction* node = newAstNodeFunction(statementsNode, parameters);
    return (AstNode*) node;
}

static AstNode* call(AstNode* leftNode, int expression_level) {
	PointerArray arguments;
	init_pointer_array(&arguments);

	while (!match(TOKEN_RIGHT_PAREN)) {
		do {
			AstNode* argument = parse_expression(PREC_ASSIGNMENT, expression_level + 1);
			write_pointer_array(&arguments, argument);
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
	init_pointer_array(&elsif_clauses);

	while (match(TOKEN_ELSIF)) {
		AstNodeStatements* body;
		AstNode* condition;
		conditioned_clause(&body, &condition, 0);
		write_pointer_array(&elsif_clauses, condition);
		write_pointer_array(&elsif_clauses, body);
	}

	AstNode* else_body = NULL;
	if (match(TOKEN_ELSE)) {
		consume(TOKEN_LEFT_BRACE, "Expected '{' to open else-body.");
		else_body = statements();
		consume(TOKEN_RIGHT_BRACE, "Expected '}' at end of else-body.");
	}
	return (AstNode*) new_ast_node_ff(condition, (AstNodeStatements*) body, elsif_clauses, (AstNodeStatements*) else_body);
}

static AstNode* while_statement(void) {
	AstNodeStatements* body;
	AstNode* condition;
	conditioned_clause(&body, &condition, 0);

	return (AstNode*) new_ast_node_while(condition, body);
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
    ParseRule prefix_rule = get_rule(parser.previous.type);
    if (prefix_rule.prefix == NULL) {
        error("Expecting a prefix operator."); // TODO: better message
        return NULL;
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
    
    AstNodeAssignment* node = ALLOCATE_AST_NODE(AstNodeAssignment, AST_NODE_ASSIGNMENT);
    node->name = variable_name;
    node->length = variable_length;
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
        	child_node = (AstNode*) return_statement();
        } else if (match(TOKEN_IF)) {
        	child_node = (AstNode*) if_statement();
        } else if (match(TOKEN_WHILE)) {
        	child_node = (AstNode*) while_statement();
    	} else {
    		AstNode* expression_or_attr_assignment = parse_expression(PREC_ASSIGNMENT, 0);
    		if (expression_or_attr_assignment->type == AST_NODE_ATTRIBUTE_ASSIGNMENT) {
    			child_node = expression_or_attr_assignment;
    		} else {
    			child_node = (AstNode*) new_ast_node_expr_statement(expression_or_attr_assignment);
    		}
        }

        write_pointer_array(&statements_node->statements, child_node);

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

AstNode* parse(const char* source) {
    init_scanner(source);
    
    advance();
    parser.had_error = false;
    
    AstNode* node = statements();
    
    return node;
}
