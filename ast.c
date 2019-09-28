#include <stdio.h>

#include "ast.h"
#include "memory.h"
#include "object.h"
#include "value.h"

const char* AST_NODE_TYPE_NAMES[] = {
    "AST_NODE_CONSTANT",
    "AST_NODE_BINARY",
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
	"AST_NODE_AND",
	"AST_NODE_OR",
	"AST_NODE_ATTRIBUTE",
	"AST_NODE_ATTRIBUTE_ASSIGNMENT",
	"AST_NODE_STRING",
	"AST_NODE_KEY_ACCESS",
	"AST_NODE_KEY_ASSIGNMENT",
	"AST_NODE_TABLE"
};

static void printNestingString(int nesting) {
	printf("\n");
	for (int i = 0; i < nesting; i++) {
        printf(". . ");
    }
}

static void printNode(AstNode* node, int nesting) {
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            printNestingString(nesting);
            printf("CONSTANT: ");
            printValue(nodeConstant->value);
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
                case TOKEN_EQUAL_EQUAL: operator = "=="; break;
                case TOKEN_GREATER_EQUAL: operator = ">="; break;
                case TOKEN_LESS_EQUAL: operator = "<="; break;
                case TOKEN_LESS_THAN: operator = "<"; break;
                case TOKEN_GREATER_THAN: operator = ">"; break;
                case TOKEN_BANG_EQUAL: operator = "!="; break;
                default: operator = "Unrecognized";
            }
            
            printNestingString(nesting);
            printf("BINARY: %s\n", operator);
            printNestingString(nesting);
            printf("Left:\n");
            printNode(nodeBinary->leftOperand, nesting + 1);
            printNestingString(nesting);
            printf("Right:\n");
            printNode(nodeBinary->rightOperand, nesting + 1);
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            printNestingString(nesting);
            printf("UNARY\n");
            printNode(nodeUnary->operand, nesting + 1);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            printNestingString(nesting);
            printf("VARIABLE: %.*s\n", nodeVariable->length, nodeVariable->name);
            break;
        }
        
        case AST_NODE_ATTRIBUTE: {
			AstNodeAttribute* node_attr = (AstNodeAttribute*) node;
			printNestingString(nesting);
			printf("ATTRIBUTE: %.*s\n", node_attr->length, node_attr->name);
			printNestingString(nesting);
			printf("Of object:\n");
			printNode(node_attr->object, nesting + 1);
			break;
		}

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
        	AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;
			printNestingString(nesting);
			printf("ATTRIBUTE_ASSIGNMENT: %.*s\n", node_attr->length, node_attr->name);
			printNestingString(nesting);
			printf("Of object:\n");
			printNode(node_attr->object, nesting + 1);
			printNestingString(nesting);
			printf("Value:\n");
			printNode(node_attr->value, nesting + 1);
			break;
		}

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;
			printNestingString(nesting);
			printf("KEY_ASSIGNMENT\n");
			printNestingString(nesting);
			printf("Of object:\n");
			printNode(node_key_assignment->subject, nesting + 1);
			printNestingString(nesting);
			printf("Key:\n");
			printNode(node_key_assignment->key, nesting + 1);
			printNestingString(nesting);
			printf("Value:\n");
			printNode(node_key_assignment->value, nesting + 1);

        	break;
        }

        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            printNestingString(nesting);
            printf("ASSIGNMENT: ");
            printf("%.*s\n", nodeAssignment->length, nodeAssignment->name);
            printNode(nodeAssignment->value, nesting + 1);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            printNestingString(nesting);
            printf("STATEMENTS\n");
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                printNode((AstNode*) nodeStatements->statements.values[i], nesting + 1);
            }
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            printNestingString(nesting);
            printf("FUNCTION\n");
            printNestingString(nesting);
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
			printNestingString(nesting);
			printf("Statements:\n");
            PointerArray* statements = &nodeFunction->statements->statements;
            for (int i = 0; i < statements->count; i++) {
                printNode((AstNode*) statements->values[i], nesting + 1);
            }
            
            break;
        }
        
        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;
            printNestingString(nesting);
            printf("TABLE\n");

            for (int i = 0; i < node_table->pairs.count; i++) {
            	AstNodesKeyValuePair pair = node_table->pairs.values[i];
                printNestingString(nesting);
				printf("Key:\n");
				printNode(pair.key, nesting + 1);
                printNestingString(nesting);
                printf("Value:\n");
                printNode(pair.value, nesting + 1);
			}

            break;
        }

        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            printNestingString(nesting);
            printf("CALL\n");
            printNode((AstNode*) nodeCall->callTarget, nesting + 1);
            printNestingString(nesting);
            printf("Arguments:\n");
            for (int i = 0; i < nodeCall->arguments.count; i++) {
				printNode(nodeCall->arguments.values[i], nesting + 1);
			}
            
            break;
        }

        case AST_NODE_KEY_ACCESS: {
            AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;
            printNestingString(nesting);
            printf("KEY_ACCESS\n");
            printNestingString(nesting);
            printf("On object:\n");
            printNode((AstNode*) node_key_access->subject, nesting + 1);
            printNestingString(nesting);
            printf("Key:\n");
			printNode(node_key_access->key, nesting + 1);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
			printNestingString(nesting);
			printf("EXPR_STATEMENT\n");
			printNode((AstNode*) nodeExprStatement->expression, nesting + 1);
			break;
		}

        case AST_NODE_RETURN: {
			AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
			printNestingString(nesting);
			printf("RETURN\n");
			printNode((AstNode*) nodeReturn->expression, nesting + 1);
			break;
		}

        case AST_NODE_IF: {
			AstNodeIf* nodeIf = (AstNodeIf*) node;
			printNestingString(nesting);
			printf("IF\n");
			printNestingString(nesting);
			printf("Condition:\n");
			printNode((AstNode*) nodeIf->condition, nesting + 1);
			printNestingString(nesting);
			printf("Body:\n");
			printNode((AstNode*) nodeIf->body, nesting + 1);

			for (int i = 0; i < nodeIf->elsifClauses.count; i += 2) {
				printNestingString(nesting);
				printf("Elsif condition:\n");
				printNode((AstNode*) nodeIf->elsifClauses.values[i], nesting + 1);
				printNestingString(nesting);
				printf("Elsif body:\n");
				printNode((AstNode*) nodeIf->elsifClauses.values[i+1], nesting + 1);
			}

			if (nodeIf->elseBody != NULL) {
				printNestingString(nesting);
				printf("Else:\n");
				printNode((AstNode*) nodeIf->body, nesting + 1);
			}
			break;
		}

        case AST_NODE_WHILE: {
        	AstNodeWhile* nodeWhile = (AstNodeWhile*) node;
        	printNestingString(nesting);
			printf("WHILE\n");
			printNestingString(nesting);
			printf("Condition:\n");
			printNode((AstNode*) nodeWhile->condition, nesting + 1);
			printNestingString(nesting);
			printf("Body:\n");
			printNode((AstNode*) nodeWhile->body, nesting + 1);
        	break;
        }

        case AST_NODE_AND: {
        	AstNodeAnd* nodeAnd = (AstNodeAnd*) node;
        	printNestingString(nesting);
			printf("AND\n");
			printNestingString(nesting);
			printf("Left:\n");
			printNode((AstNode*) nodeAnd->left, nesting + 1);
			printNestingString(nesting);
			printf("Right:\n");
			printNode((AstNode*) nodeAnd->right, nesting + 1);
        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* nodeOr = (AstNodeOr*) node;
        	printNestingString(nesting);
			printf("OR\n");
			printNestingString(nesting);
			printf("Left:\n");
			printNode((AstNode*) nodeOr->left, nesting + 1);
			printNestingString(nesting);
			printf("Right:\n");
			printNode((AstNode*) nodeOr->right, nesting + 1);
        	break;
        }

        case AST_NODE_STRING: {
			AstNodeString* node_string = (AstNodeString*) node;
			printNestingString(nesting);
			printf("STRING: %.*s\n", node_string->length, node_string->string);
			break;
		}
    }
}

void printTree(AstNode* tree) {
    printNode(tree, 0);
}

static void freeNode(AstNode* node, int nesting) {
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
        
        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;
            freeNode(node_attr ->object, nesting + 1);
            deallocate(node_attr, sizeof(AstNodeAttribute), deallocationString);
            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
        	AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;
			freeNode(node_attr->object, nesting + 1);
			freeNode(node_attr->value, nesting + 1);
			deallocate(node_attr, sizeof(AstNodeAttributeAssignment), deallocationString);
        	break;
        }

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;
			freeNode(node_key_assignment->subject, nesting + 1);
			freeNode(node_key_assignment->key, nesting + 1);
			freeNode(node_key_assignment->value, nesting + 1);
			deallocate(node_key_assignment, sizeof(AstNodeKeyAssignment), deallocationString);
			break;
        }

        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            freeNode(nodeBinary->leftOperand, nesting + 1);
            freeNode(nodeBinary->rightOperand, nesting + 1);
            
            deallocate(nodeBinary, sizeof(AstNodeBinary), deallocationString);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            
            freeNode(nodeUnary->operand, nesting + 1);
            
            deallocate(nodeUnary, sizeof(AstNodeUnary), deallocationString);
            
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssingment = (AstNodeAssignment*) node;
            
            // not freeing the cstring in the ast node because its part of the source code, to be freed later
            freeNode(nodeAssingment->value, nesting + 1);
            
            deallocate(nodeAssingment, sizeof(AstNodeAssignment), deallocationString);
            
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                freeNode((AstNode*) nodeStatements->statements.values[i], nesting + 1);
            }
            
            freePointerArray(&nodeStatements->statements);
            deallocate(nodeStatements, sizeof(AstNodeStatements), deallocationString);
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            freeNode((AstNode*) nodeFunction->statements, nesting + 1);
            value_array_free(&nodeFunction->parameters);
            deallocate(nodeFunction, sizeof(AstNodeFunction), deallocationString);
            break;
        }
        
        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;

        	for (int i = 0; i < node_table->pairs.count; i++) {
        		AstNodesKeyValuePair pair = node_table->pairs.values[i];
        		freeNode(pair.key, nesting + 1);
        		freeNode(pair.value, nesting + 1);
			}

        	ast_key_value_pair_array_free(&node_table->pairs);
			deallocate(node_table, sizeof(AstNodeTable), deallocationString);
			break;
        }

        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            freeNode((AstNode*) nodeCall->callTarget, nesting + 1);
            for (int i = 0; i < nodeCall->arguments.count; i++) {
				freeNode(nodeCall->arguments.values[i], nesting + 1);
			}
            freePointerArray(&nodeCall->arguments);
            deallocate(nodeCall, sizeof(AstNodeCall), deallocationString);
            break;
        }

        case AST_NODE_KEY_ACCESS: {
            AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;
            freeNode((AstNode*) node_key_access->subject, nesting + 1);
			freeNode(node_key_access->key, nesting + 1);
            deallocate(node_key_access, sizeof(AstNodeKeyAccess), deallocationString);
            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
            AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
            freeNode((AstNode*) nodeExprStatement->expression, nesting + 1);
            deallocate(nodeExprStatement, sizeof(AstNodeExprStatement), deallocationString);
            break;
        }

        case AST_NODE_RETURN: {
			AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
			freeNode((AstNode*) nodeReturn->expression, nesting + 1);
			deallocate(nodeReturn, sizeof(AstNodeReturn), deallocationString);
			break;
		}

        case AST_NODE_IF: {
			AstNodeIf* nodeIf = (AstNodeIf*) node;
			freeNode((AstNode*) nodeIf->condition, nesting + 1);
			freeNode((AstNode*) nodeIf->body, nesting + 1);
			if (nodeIf->elseBody != NULL) {
				freeNode((AstNode*) nodeIf->elseBody, nesting + 1);
			}
			for (int i = 0; i < nodeIf->elsifClauses.count; i++) {
				freeNode(nodeIf->elsifClauses.values[i], nesting + 1);
			}
			freePointerArray(&nodeIf->elsifClauses);
			deallocate(nodeIf, sizeof(AstNodeIf), deallocationString);
			break;
		}

        case AST_NODE_WHILE: {
			AstNodeWhile* nodeWhile = (AstNodeWhile*) node;
			freeNode((AstNode*) nodeWhile->condition, nesting + 1);
			freeNode((AstNode*) nodeWhile->body, nesting + 1);
			deallocate(nodeWhile, sizeof(AstNodeWhile), deallocationString);
			break;
		}

        case AST_NODE_AND: {
			AstNodeAnd* nodeAnd = (AstNodeAnd*) node;
			freeNode((AstNode*) nodeAnd->left, nesting + 1);
			freeNode((AstNode*) nodeAnd->right, nesting + 1);
			deallocate(nodeAnd, sizeof(AstNodeAnd), deallocationString);
			break;
		}

        case AST_NODE_OR: {
			AstNodeOr* nodeOr = (AstNodeOr*) node;
			freeNode((AstNode*) nodeOr->left, nesting + 1);
			freeNode((AstNode*) nodeOr->right, nesting + 1);
			deallocate(nodeOr, sizeof(AstNodeOr), deallocationString);
			break;
		}


        case AST_NODE_STRING: {
			AstNodeString* node_string = (AstNodeString*) node;
			deallocate(node_string, sizeof(AstNodeString), deallocationString);
			break;
		}
    }
}

void freeTree(AstNode* node) {
    DEBUG_MEMORY("Freeing AST");
    freeNode(node, 0);
}

AstNodeStatements* newAstNodeStatements(void) {
    AstNodeStatements* node = ALLOCATE_AST_NODE(AstNodeStatements, AST_NODE_STATEMENTS);
    init_pointer_array(&node->statements);
    return node;
}

AstNodeExprStatement* new_ast_node_expr_statement(AstNode* expression) {
	AstNodeExprStatement* node = ALLOCATE_AST_NODE(AstNodeExprStatement, AST_NODE_EXPR_STATEMENT);
	node->expression = expression;
	return node;
}

AstNodeCall* newAstNodeCall(AstNode* callTarget, PointerArray arguments) {
	AstNodeCall* node = ALLOCATE_AST_NODE(AstNodeCall, AST_NODE_CALL);
	node->callTarget = callTarget;
	node->arguments = arguments;
	return node;
}

AstNodeReturn* newAstNodeReturn(AstNode* expression) {
	AstNodeReturn* node = ALLOCATE_AST_NODE(AstNodeReturn, AST_NODE_RETURN);
	node->expression = expression;
	return node;
}

AstNodeFunction* newAstNodeFunction(AstNodeStatements* statements, ValueArray parameters) {
	AstNodeFunction* node = ALLOCATE_AST_NODE(AstNodeFunction, AST_NODE_FUNCTION);
	node->statements = statements;
	node->parameters = parameters;
	return node;
}

AstNodeConstant* newAstNodeConstant(Value value) {
	AstNodeConstant* node = ALLOCATE_AST_NODE(AstNodeConstant, AST_NODE_CONSTANT);
	node->value = value;
	return node;
}

AstNodeIf* new_ast_node_ff(AstNode* condition, AstNodeStatements* body, PointerArray elsifClauses, AstNodeStatements* elseBody) {
	AstNodeIf* node = ALLOCATE_AST_NODE(AstNodeIf, AST_NODE_IF);
	node->condition = condition;
	node->body = body;
	node->elsifClauses = elsifClauses;
	node->elseBody = elseBody;
	return node;
}

AstNodeWhile* new_ast_node_while(AstNode* condition, AstNodeStatements* body) {
	AstNodeWhile* node = ALLOCATE_AST_NODE(AstNodeWhile, AST_NODE_WHILE);
	node->condition = condition;
	node->body = body;
	return node;
}

AstNodeAnd* new_ast_node_and(AstNode* left, AstNode* right) {
	AstNodeAnd* node = ALLOCATE_AST_NODE(AstNodeAnd, AST_NODE_AND);
	node->left = left;
	node->right = right;
	return node;
}

AstNodeOr* new_ast_node_or(AstNode* left, AstNode* right) {
	AstNodeOr* node = ALLOCATE_AST_NODE(AstNodeOr, AST_NODE_OR);
	node->left = left;
	node->right = right;
	return node;
}

AstNodeAttribute* new_ast_node_attribute(AstNode* object, const char* name, int length) {
	AstNodeAttribute* node = ALLOCATE_AST_NODE(AstNodeAttribute, AST_NODE_ATTRIBUTE);
	node->object = object;
	node->name = name;
	node->length = length;
	return node;
}

AstNodeAttributeAssignment* new_ast_node_attribute_assignment(AstNode* object, const char* name, int length, AstNode* value) {
	AstNodeAttributeAssignment* node = ALLOCATE_AST_NODE(AstNodeAttributeAssignment, AST_NODE_ATTRIBUTE_ASSIGNMENT);
	node->object = object;
	node->name = name;
	node->length = length;
	node->value = value;
	return node;
}

AstNodeString* new_ast_node_string(const char* string, int length) {
	AstNodeString* node = ALLOCATE_AST_NODE(AstNodeString, AST_NODE_STRING);
	node->string = string;
	node->length = length;
	return node;
}

AstNodeKeyAccess* new_ast_node_key_access(AstNode* key, AstNode* subject) {
	AstNodeKeyAccess* node = ALLOCATE_AST_NODE(AstNodeKeyAccess, AST_NODE_KEY_ACCESS);
	node->key = key;
	node->subject = subject;
	return node;
}

AstNodeKeyAssignment* new_ast_node_key_assignment(AstNode* key, AstNode* value, AstNode* subject) {
	AstNodeKeyAssignment* node = ALLOCATE_AST_NODE(AstNodeKeyAssignment, AST_NODE_KEY_ASSIGNMENT);
	node->key = key;
	node->value = value;
	node->subject = subject;
	return node;
}

AstNodeUnary* new_ast_node_unary(AstNode* expression) {
	AstNodeUnary* node = ALLOCATE_AST_NODE(AstNodeUnary, AST_NODE_UNARY);
	node->operand = expression;
	return node;
}

AstNodeTable* new_ast_node_table(AstKeyValuePairArray pairs) {
	AstNodeTable* node = ALLOCATE_AST_NODE(AstNodeTable, AST_NODE_TABLE);
	node->pairs = pairs;
	return node;
}

AstNode* allocateAstNode(AstNodeType type, size_t size) {
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
