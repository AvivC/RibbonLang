#include <stdio.h>

#include "ast.h"
#include "memory.h"
#include "plane_object.h"
#include "value.h"

const char* AST_NODE_TYPE_NAMES[] = {
    "AST_NODE_CONSTANT",
    "AST_NODE_BINARY",
	"AST_NODE_IN_PLACE_ATTRIBUTE_BINARY",
    "AST_NODE_UNARY",
    "AST_NODE_VARIABLE",
    "AST_NODE_ASSIGNMENT",
    "AST_NODE_STATEMENTS",
    "AST_NODE_FUNCTION",
    "AST_NODE_CALL",
    "AST_NODE_EXPR_STATEMENT",
	"AST_NODE_RETURN",
	"AST_NODE_IF",
	"AST_NODE_WHILE",
	"AST_NODE_FOR",
	"AST_NODE_AND",
	"AST_NODE_OR",
	"AST_NODE_ATTRIBUTE",
	"AST_NODE_ATTRIBUTE_ASSIGNMENT",
	"AST_NODE_STRING",
	"AST_NODE_KEY_ACCESS",
	"AST_NODE_KEY_ASSIGNMENT",
	"AST_NODE_TABLE",
	"AST_NODE_IMPORT",
	"AST_NODE_CLASS",
	"AST_NODE_NIL"
};

static void print_nesting_string(int nesting) {
	printf("\n");
	for (int i = 0; i < nesting; i++) {
        printf(". . ");
    }
}

static void print_node(AstNode* node, int nesting) {
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            print_nesting_string(nesting);
            printf("CONSTANT: ");
            value_print(nodeConstant->value);
            printf("\n");
            break;
        }
        
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            const char* operator = NULL;
            switch (nodeBinary->operator) {
                case TOKEN_PLUS: operator = "+"; break;
                case TOKEN_MINUS: operator = "-"; break;
                case TOKEN_STAR: operator = "*"; break;
                case TOKEN_SLASH: operator = "/"; break;
                case TOKEN_MODULO: operator = "%"; break;
                case TOKEN_EQUAL_EQUAL: operator = "=="; break;
                case TOKEN_GREATER_EQUAL: operator = ">="; break;
                case TOKEN_LESS_EQUAL: operator = "<="; break;
                case TOKEN_LESS_THAN: operator = "<"; break;
                case TOKEN_GREATER_THAN: operator = ">"; break;
                case TOKEN_BANG_EQUAL: operator = "!="; break;
                default: operator = "Unrecognized";
            }
            
            print_nesting_string(nesting);
            printf("BINARY: %s\n", operator);
            print_nesting_string(nesting);
            printf("Left:\n");
            print_node(nodeBinary->left_operand, nesting + 1);
            print_nesting_string(nesting);
            printf("Right:\n");
            print_node(nodeBinary->right_operand, nesting + 1);
            break;
        }

		case AST_NODE_IN_PLACE_ATTRIBUTE_BINARY: {
			AstNodeInPlaceAttributeBinary* in_place_node = (AstNodeInPlaceAttributeBinary*) node;

			const char* operator = NULL;
            switch (in_place_node->operator) {
                case TOKEN_PLUS_EQUALS: operator = "+="; break;
                case TOKEN_MINUS_EQUALS: operator = "-="; break;
                case TOKEN_STAR_EQUALS: operator = "*="; break;
                case TOKEN_SLASH_EQUALS: operator = "/="; break;
                case TOKEN_MODULO_EQUALS: operator = "%="; break;
                default: FAIL("Unrecognized operator in AST_NODE_IN_PLACE_ATTRIBUTE_BINARY: %d", in_place_node->operator);
            }
            
            print_nesting_string(nesting);
            printf("IN_PLACE_ATTRIBUTE_BINARY: %s\n", operator);
            print_nesting_string(nesting);
            printf("Attribute: %.*s\n", in_place_node->attribute_length, in_place_node->attribute);
            print_nesting_string(nesting);
            printf("Of object:\n");
            print_node(in_place_node->subject, nesting + 1);
			print_nesting_string(nesting);
            printf("With value:\n");
			print_node(in_place_node->value, nesting + 1);

			break;
		}
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            print_nesting_string(nesting);
            printf("UNARY\n");
            print_node(nodeUnary->operand, nesting + 1);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            print_nesting_string(nesting);
            printf("VARIABLE: %.*s\n", nodeVariable->length, nodeVariable->name);
            break;
        }
        
        case AST_NODE_ATTRIBUTE: {
			AstNodeAttribute* node_attr = (AstNodeAttribute*) node;
			print_nesting_string(nesting);
			printf("ATTRIBUTE: %.*s\n", node_attr->length, node_attr->name);
			print_nesting_string(nesting);
			printf("Of object:\n");
			print_node(node_attr->object, nesting + 1);
			break;
		}

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
        	AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;
			print_nesting_string(nesting);
			printf("ATTRIBUTE_ASSIGNMENT: %.*s\n", node_attr->length, node_attr->name);
			print_nesting_string(nesting);
			printf("Of object:\n");
			print_node(node_attr->object, nesting + 1);
			print_nesting_string(nesting);
			printf("Value:\n");
			print_node(node_attr->value, nesting + 1);
			break;
		}

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;
			print_nesting_string(nesting);
			printf("KEY_ASSIGNMENT\n");
			print_nesting_string(nesting);
			printf("Of object:\n");
			print_node(node_key_assignment->subject, nesting + 1);
			print_nesting_string(nesting);
			printf("Key:\n");
			print_node(node_key_assignment->key, nesting + 1);
			print_nesting_string(nesting);
			printf("Value:\n");
			print_node(node_key_assignment->value, nesting + 1);

        	break;
        }

        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            print_nesting_string(nesting);
            printf("ASSIGNMENT: ");
            printf("%.*s\n", nodeAssignment->length, nodeAssignment->name);
            print_node(nodeAssignment->value, nesting + 1);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            print_nesting_string(nesting);
            printf("STATEMENTS\n");
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                print_node((AstNode*) nodeStatements->statements.values[i], nesting + 1);
            }
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            print_nesting_string(nesting);
            printf("FUNCTION\n");
            print_nesting_string(nesting);
			printf("Parameters: ");
			ValueArray parameters = nodeFunction->parameters;
			if (parameters.count == 0) {
				printf("-- No parameters --");
			} else {
				for (int i = 0; i < parameters.count; i++) {
					RawString parameter = parameters.values[i].as.raw_string;
					printf("%.*s", parameter.length, parameter.data);
					if (i != parameters.count - 1) {
						printf(", ");
					}
				}
			}
			printf("\n");
			print_nesting_string(nesting);
			printf("Statements:\n");
            PointerArray* statements = &nodeFunction->statements->statements;
            for (int i = 0; i < statements->count; i++) {
                print_node((AstNode*) statements->values[i], nesting + 1);
            }
            
            break;
        }
        
        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;
            print_nesting_string(nesting);
            printf("TABLE\n");

            for (int i = 0; i < node_table->pairs.count; i++) {
            	AstNodesKeyValuePair pair = node_table->pairs.values[i];
                print_nesting_string(nesting);
				printf("Key:\n");
				print_node(pair.key, nesting + 1);
                print_nesting_string(nesting);
                printf("Value:\n");
                print_node(pair.value, nesting + 1);
			}

            break;
        }

        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            print_nesting_string(nesting);
            printf("CALL\n");
            print_node((AstNode*) nodeCall->target, nesting + 1);
            print_nesting_string(nesting);
            printf("Arguments:\n");
            for (int i = 0; i < nodeCall->arguments.count; i++) {
				print_node(nodeCall->arguments.values[i], nesting + 1);
			}
            
            break;
        }

        case AST_NODE_IMPORT: {
            AstNodeImport* node_import = (AstNodeImport*) node;
            print_nesting_string(nesting);
            printf("IMPORT: %.*s\n", node_import->name_length, node_import->name);

            break;
        }

        case AST_NODE_KEY_ACCESS: {
            AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;
            print_nesting_string(nesting);
            printf("KEY_ACCESS\n");
            print_nesting_string(nesting);
            printf("On object:\n");
            print_node((AstNode*) node_key_access->subject, nesting + 1);
            print_nesting_string(nesting);
            printf("Key:\n");
			print_node(node_key_access->key, nesting + 1);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
			print_nesting_string(nesting);
			printf("EXPR_STATEMENT\n");
			print_node((AstNode*) nodeExprStatement->expression, nesting + 1);
			break;
		}

        case AST_NODE_RETURN: {
			AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
			print_nesting_string(nesting);
			printf("RETURN\n");
			print_node((AstNode*) nodeReturn->expression, nesting + 1);
			break;
		}

        case AST_NODE_IF: {
			AstNodeIf* nodeIf = (AstNodeIf*) node;
			print_nesting_string(nesting);
			printf("IF\n");
			print_nesting_string(nesting);
			printf("Condition:\n");
			print_node((AstNode*) nodeIf->condition, nesting + 1);
			print_nesting_string(nesting);
			printf("Body:\n");
			print_node((AstNode*) nodeIf->body, nesting + 1);

			for (int i = 0; i < nodeIf->elsif_clauses.count; i += 2) {
				print_nesting_string(nesting);
				printf("Elsif condition:\n");
				print_node((AstNode*) nodeIf->elsif_clauses.values[i], nesting + 1);
				print_nesting_string(nesting);
				printf("Elsif body:\n");
				print_node((AstNode*) nodeIf->elsif_clauses.values[i+1], nesting + 1);
			}

			if (nodeIf->else_body != NULL) {
				print_nesting_string(nesting);
				printf("Else:\n");
				print_node((AstNode*) nodeIf->body, nesting + 1);
			}
			break;
		}

        case AST_NODE_WHILE: {
        	AstNodeWhile* nodeWhile = (AstNodeWhile*) node;
        	print_nesting_string(nesting);
			printf("WHILE\n");
			print_nesting_string(nesting);
			printf("Condition:\n");
			print_node((AstNode*) nodeWhile->condition, nesting + 1);
			print_nesting_string(nesting);
			printf("Body:\n");
			print_node((AstNode*) nodeWhile->body, nesting + 1);
        	break;
        }

		case AST_NODE_FOR: {
        	AstNodeFor* node_for = (AstNodeFor*) node;
        	print_nesting_string(nesting);
			printf("FOR\n");
			print_nesting_string(nesting);
			printf("Variable: %.*s\n", node_for->variable_length, node_for->variable_name);
			print_nesting_string(nesting);
			printf("Container:\n");
			print_node((AstNode*) node_for->container, nesting + 1);
			print_nesting_string(nesting);
			printf("Body:\n");
			print_node((AstNode*) node_for->body, nesting + 1);
        	break;
        }

        case AST_NODE_AND: {
        	AstNodeAnd* nodeAnd = (AstNodeAnd*) node;
        	print_nesting_string(nesting);
			printf("AND\n");
			print_nesting_string(nesting);
			printf("Left:\n");
			print_node((AstNode*) nodeAnd->left, nesting + 1);
			print_nesting_string(nesting);
			printf("Right:\n");
			print_node((AstNode*) nodeAnd->right, nesting + 1);
        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* nodeOr = (AstNodeOr*) node;
        	print_nesting_string(nesting);
			printf("OR\n");
			print_nesting_string(nesting);
			printf("Left:\n");
			print_node((AstNode*) nodeOr->left, nesting + 1);
			print_nesting_string(nesting);
			printf("Right:\n");
			print_node((AstNode*) nodeOr->right, nesting + 1);
        	break;
        }

        case AST_NODE_STRING: {
			AstNodeString* node_string = (AstNodeString*) node;
			print_nesting_string(nesting);
			printf("STRING: %.*s\n", node_string->length, node_string->string);
			break;
		}

		case AST_NODE_CLASS: {
			AstNodeClass* node_class = (AstNodeClass*) node;
			print_nesting_string(nesting);
			printf("CLASS\n");
			print_nesting_string(nesting);
			printf("Statements:\n");
            PointerArray* statements = &node_class->body->statements;
            for (int i = 0; i < statements->count; i++) {
                print_node((AstNode*) statements->values[i], nesting + 1);
            }
			break;
		}
		case AST_NODE_NIL: {
			print_nesting_string(nesting);
			printf("NIL\n");
			break;
		}
    }
}

void ast_print_tree(AstNode* tree) {
    print_node(tree, 0);
}

static void node_free(AstNode* node, int nesting) {
    const char* deallocationString = AST_NODE_TYPE_NAMES[node->type];
    
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            deallocate(nodeConstant, sizeof(AstNodeConstant), deallocationString);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            deallocate(nodeVariable, sizeof(AstNodeVariable), deallocationString);
            break;
        }
        
        case AST_NODE_IMPORT: {
        	AstNodeImport* node_import = (AstNodeImport*) node;
        	// The name in the node points into the source code, do not free it. Maybe change that later?
			deallocate(node_import, sizeof(AstNodeImport), deallocationString);
        	break;
        }

        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;
            node_free(node_attr ->object, nesting + 1);
            deallocate(node_attr, sizeof(AstNodeAttribute), deallocationString);
            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
        	AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;
			node_free(node_attr->object, nesting + 1);
			node_free(node_attr->value, nesting + 1);
			deallocate(node_attr, sizeof(AstNodeAttributeAssignment), deallocationString);
        	break;
        }

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;
			node_free(node_key_assignment->subject, nesting + 1);
			node_free(node_key_assignment->key, nesting + 1);
			node_free(node_key_assignment->value, nesting + 1);
			deallocate(node_key_assignment, sizeof(AstNodeKeyAssignment), deallocationString);
			break;
        }

        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            node_free(nodeBinary->left_operand, nesting + 1);
            node_free(nodeBinary->right_operand, nesting + 1);
            
            deallocate(nodeBinary, sizeof(AstNodeBinary), deallocationString);
            
            break;
        }

		case AST_NODE_IN_PLACE_ATTRIBUTE_BINARY: {
            AstNodeInPlaceAttributeBinary* node_in_place = (AstNodeInPlaceAttributeBinary*) node;
            
            node_free(node_in_place->subject, nesting + 1);
            node_free(node_in_place->value, nesting + 1);
            
            deallocate(node_in_place, sizeof(AstNodeInPlaceAttributeBinary), deallocationString);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            
            node_free(nodeUnary->operand, nesting + 1);
            
            deallocate(nodeUnary, sizeof(AstNodeUnary), deallocationString);
            
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssingment = (AstNodeAssignment*) node;
            
            // not freeing the cstring in the ast node because its part of the source code, to be freed later
            node_free(nodeAssingment->value, nesting + 1);
            
            deallocate(nodeAssingment, sizeof(AstNodeAssignment), deallocationString);
            
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                node_free((AstNode*) nodeStatements->statements.values[i], nesting + 1);
            }
            
            pointer_array_free(&nodeStatements->statements);
            deallocate(nodeStatements, sizeof(AstNodeStatements), deallocationString);
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            node_free((AstNode*) nodeFunction->statements, nesting + 1);
            value_array_free(&nodeFunction->parameters);
            deallocate(nodeFunction, sizeof(AstNodeFunction), deallocationString);
            break;
        }
        
        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;

        	for (int i = 0; i < node_table->pairs.count; i++) {
        		AstNodesKeyValuePair pair = node_table->pairs.values[i];
        		node_free(pair.key, nesting + 1);
        		node_free(pair.value, nesting + 1);
			}

        	ast_key_value_pair_array_free(&node_table->pairs);
			deallocate(node_table, sizeof(AstNodeTable), deallocationString);
			break;
        }

        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            node_free((AstNode*) nodeCall->target, nesting + 1);
            for (int i = 0; i < nodeCall->arguments.count; i++) {
				node_free(nodeCall->arguments.values[i], nesting + 1);
			}
            pointer_array_free(&nodeCall->arguments);
            deallocate(nodeCall, sizeof(AstNodeCall), deallocationString);
            break;
        }

        case AST_NODE_KEY_ACCESS: {
            AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;
            node_free((AstNode*) node_key_access->subject, nesting + 1);
			node_free(node_key_access->key, nesting + 1);
            deallocate(node_key_access, sizeof(AstNodeKeyAccess), deallocationString);
            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
            AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
            node_free((AstNode*) nodeExprStatement->expression, nesting + 1);
            deallocate(nodeExprStatement, sizeof(AstNodeExprStatement), deallocationString);
            break;
        }

        case AST_NODE_RETURN: {
			AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
			node_free((AstNode*) nodeReturn->expression, nesting + 1);
			deallocate(nodeReturn, sizeof(AstNodeReturn), deallocationString);
			break;
		}

        case AST_NODE_IF: {
			AstNodeIf* nodeIf = (AstNodeIf*) node;
			node_free((AstNode*) nodeIf->condition, nesting + 1);
			node_free((AstNode*) nodeIf->body, nesting + 1);
			if (nodeIf->else_body != NULL) {
				node_free((AstNode*) nodeIf->else_body, nesting + 1);
			}
			for (int i = 0; i < nodeIf->elsif_clauses.count; i++) {
				node_free(nodeIf->elsif_clauses.values[i], nesting + 1);
			}
			pointer_array_free(&nodeIf->elsif_clauses);
			deallocate(nodeIf, sizeof(AstNodeIf), deallocationString);
			break;
		}

        case AST_NODE_WHILE: {
			AstNodeWhile* nodeWhile = (AstNodeWhile*) node;
			node_free((AstNode*) nodeWhile->condition, nesting + 1);
			node_free((AstNode*) nodeWhile->body, nesting + 1);
			deallocate(nodeWhile, sizeof(AstNodeWhile), deallocationString);
			break;
		}

		case AST_NODE_FOR: {
			AstNodeFor* node_for = (AstNodeFor*) node;
			node_free((AstNode*) node_for->container, nesting + 1);
			node_free((AstNode*) node_for->body, nesting + 1);
			deallocate(node_for, sizeof(AstNodeFor), deallocationString);
			break;
		}

        case AST_NODE_AND: {
			AstNodeAnd* nodeAnd = (AstNodeAnd*) node;
			node_free((AstNode*) nodeAnd->left, nesting + 1);
			node_free((AstNode*) nodeAnd->right, nesting + 1);
			deallocate(nodeAnd, sizeof(AstNodeAnd), deallocationString);
			break;
		}

        case AST_NODE_OR: {
			AstNodeOr* nodeOr = (AstNodeOr*) node;
			node_free((AstNode*) nodeOr->left, nesting + 1);
			node_free((AstNode*) nodeOr->right, nesting + 1);
			deallocate(nodeOr, sizeof(AstNodeOr), deallocationString);
			break;
		}


        case AST_NODE_STRING: {
			AstNodeString* node_string = (AstNodeString*) node;
			deallocate(node_string, sizeof(AstNodeString), deallocationString);
			break;
		}

		case AST_NODE_CLASS: {
			AstNodeClass* node_class = (AstNodeClass*) node;
			node_free((AstNode*) node_class->body, nesting + 1);
			deallocate(node_class, sizeof(AstNodeClass), deallocationString);
			break;
		}
		case AST_NODE_NIL: {
			AstNodeNil* node_nil = (AstNodeNil*) node;
			deallocate(node_nil, sizeof(AstNodeNil), deallocationString);
			break;
		}
    }
}

void ast_free_tree(AstNode* node) {
    DEBUG_MEMORY("Freeing AST");
    node_free(node, 0);
}

AstNodeStatements* ast_new_node_statements(void) {
    AstNodeStatements* node = ALLOCATE_AST_NODE(AstNodeStatements, AST_NODE_STATEMENTS);
    pointer_array_init(&node->statements, "AstNodeStatements pointer array buffer");
    return node;
}

AstNodeExprStatement* ast_new_node_expr_statement(AstNode* expression) {
	AstNodeExprStatement* node = ALLOCATE_AST_NODE(AstNodeExprStatement, AST_NODE_EXPR_STATEMENT);
	node->expression = expression;
	return node;
}

AstNodeVariable* ast_new_node_variable(const char* name, int length) {
	AstNodeVariable* node = ALLOCATE_AST_NODE(AstNodeVariable, AST_NODE_VARIABLE);
	node->name = name;
	node->length = length;
	return node;
}

AstNodeConstant* ast_new_node_number(double number) {
	AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
	node->value = MAKE_VALUE_NUMBER(number);
	return node;
}

AstNodeBinary* ast_new_node_binary(ScannerTokenType operator, AstNode* left_operand, AstNode* right_operand) {
	AstNodeBinary* node = ALLOCATE_AST_NODE(AstNodeBinary, AST_NODE_BINARY);
	node->operator = operator;
	node->left_operand = left_operand;
	node->right_operand = right_operand;
	return node;
}

AstNodeInPlaceAttributeBinary* ast_new_node_in_place_attribute_binary(
        ScannerTokenType operator, AstNode* subject, const char* attribute, int attribute_length, AstNode* value) {
	AstNodeInPlaceAttributeBinary* node = ALLOCATE_AST_NODE(AstNodeInPlaceAttributeBinary, AST_NODE_IN_PLACE_ATTRIBUTE_BINARY);
	node->operator = operator;
	node->subject = subject;
	node->attribute = attribute;
	node->attribute_length = attribute_length;
	node->value = value;
	return node;
}

AstNodeCall* ast_new_node_call(AstNode* callTarget, PointerArray arguments) {
	AstNodeCall* node = ALLOCATE_AST_NODE(AstNodeCall, AST_NODE_CALL);
	node->target = callTarget;
	node->arguments = arguments;
	return node;
}

AstNodeReturn* ast_new_node_return(AstNode* expression) {
	AstNodeReturn* node = ALLOCATE_AST_NODE(AstNodeReturn, AST_NODE_RETURN);
	node->expression = expression;
	return node;
}

AstNodeFunction* ast_new_node_function(AstNodeStatements* statements, ValueArray parameters) {
	AstNodeFunction* node = ALLOCATE_AST_NODE(AstNodeFunction, AST_NODE_FUNCTION);
	node->statements = statements;
	node->parameters = parameters;
	return node;
}

AstNodeConstant* ast_new_node_constant(Value value) {
	AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
	node->value = value;
	return node;
}

AstNodeNil* ast_new_node_nil(void) {
	return ALLOCATE_AST_NODE(AstNodeNil, AST_NODE_NIL);
}

AstNodeIf* ast_new_node_if(AstNode* condition, AstNodeStatements* body, PointerArray elsif_clauses, AstNodeStatements* else_body) {
	AstNodeIf* node = ALLOCATE_AST_NODE(AstNodeIf, AST_NODE_IF);
	node->condition = condition;
	node->body = body;
	node->elsif_clauses = elsif_clauses;
	node->else_body = else_body;
	return node;
}

AstNodeFor* ast_new_node_for(const char* variable_name, int variable_length, AstNode* container, AstNodeStatements* body) {
	AstNodeFor* node = ALLOCATE_AST_NODE(AstNodeFor, AST_NODE_FOR);
	node->variable_name = variable_name;
	node->variable_length = variable_length;
	node->container = container;
	node->body = body;
	return node;
}

AstNodeWhile* ast_new_node_while(AstNode* condition, AstNodeStatements* body) {
	AstNodeWhile* node = ALLOCATE_AST_NODE(AstNodeWhile, AST_NODE_WHILE);
	node->condition = condition;
	node->body = body;
	return node;
}

AstNodeAnd* ast_new_node_and(AstNode* left, AstNode* right) {
	AstNodeAnd* node = ALLOCATE_AST_NODE(AstNodeAnd, AST_NODE_AND);
	node->left = left;
	node->right = right;
	return node;
}

AstNodeAssignment* ast_new_node_assignment(const char* name, int name_length, AstNode* value) {
	AstNodeAssignment* node = ALLOCATE_AST_NODE(AstNodeAssignment, AST_NODE_ASSIGNMENT);
	node->name = name;
	node->length = name_length;
	node->value = value;
	return node;
}

AstNodeOr* ast_new_node_or(AstNode* left, AstNode* right) {
	AstNodeOr* node = ALLOCATE_AST_NODE(AstNodeOr, AST_NODE_OR);
	node->left = left;
	node->right = right;
	return node;
}

AstNodeAttribute* ast_new_node_attribute(AstNode* object, const char* name, int length) {
	AstNodeAttribute* node = ALLOCATE_AST_NODE(AstNodeAttribute, AST_NODE_ATTRIBUTE);
	node->object = object;
	node->name = name;
	node->length = length;
	return node;
}

AstNodeAttributeAssignment* ast_new_node_attribute_assignment(AstNode* object, const char* name, int length, AstNode* value) {
	AstNodeAttributeAssignment* node = ALLOCATE_AST_NODE(AstNodeAttributeAssignment, AST_NODE_ATTRIBUTE_ASSIGNMENT);
	node->object = object;
	node->name = name;
	node->length = length;
	node->value = value;
	return node;
}

AstNodeString* ast_new_node_string(const char* string, int length) {
	AstNodeString* node = ALLOCATE_AST_NODE(AstNodeString, AST_NODE_STRING);
	node->string = string;
	node->length = length;
	return node;
}

AstNodeKeyAccess* ast_new_node_key_access(AstNode* key, AstNode* subject) {
	AstNodeKeyAccess* node = ALLOCATE_AST_NODE(AstNodeKeyAccess, AST_NODE_KEY_ACCESS);
	node->key = key;
	node->subject = subject;
	return node;
}

AstNodeKeyAssignment* ast_new_node_key_assignment(AstNode* key, AstNode* value, AstNode* subject) {
	AstNodeKeyAssignment* node = ALLOCATE_AST_NODE(AstNodeKeyAssignment, AST_NODE_KEY_ASSIGNMENT);
	node->key = key;
	node->value = value;
	node->subject = subject;
	return node;
}

AstNodeUnary* ast_new_node_unary(AstNode* expression) {
	AstNodeUnary* node = ALLOCATE_AST_NODE(AstNodeUnary, AST_NODE_UNARY);
	node->operand = expression;
	return node;
}

AstNodeTable* ast_new_node_table(AstKeyValuePairArray pairs) {
	AstNodeTable* node = ALLOCATE_AST_NODE(AstNodeTable, AST_NODE_TABLE);
	node->pairs = pairs;
	return node;
}

AstNodeImport* ast_new_node_import(const char* name, int name_length) {
	AstNodeImport* node = ALLOCATE_AST_NODE(AstNodeImport, AST_NODE_IMPORT);
	node->name = name;
	node->name_length = name_length;
	return node;
}

AstNodeClass* ast_new_node_class(AstNodeStatements* body) {
	AstNodeClass* node = ALLOCATE_AST_NODE(AstNodeClass, AST_NODE_CLASS);
	node->body = body;
	return node;
}

AstNode* ast_allocate_node(AstNodeType type, size_t size) {
    AstNode* node = allocate(size, AST_NODE_TYPE_NAMES[type]);
    node->type = type;
    return node;
}

AstNodesKeyValuePair ast_new_key_value_pair(AstNode* key, AstNode* value) {
	AstNodesKeyValuePair pair;
	pair.key = key;
	pair.value = value;
	return pair;
}

IMPLEMENT_DYNAMIC_ARRAY(AstNodesKeyValuePair, AstKeyValuePairArray, ast_key_value_pair_array)
