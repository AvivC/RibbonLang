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
	"AST_NODE_RETURN"
};

static void printNestingString(int nesting) {
	printf("\n");
	for (int i = 0; i < nesting; i++) {
        printf(". . . . ");
//        printf("    ");
    }
}

static void printNode(AstNode* node, int nesting) {
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            printNestingString(nesting);
            printf("AST_NODE_CONSTANT: ");
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
                default: operator = "Unrecognized";
            }
            
            printNestingString(nesting);
            printf("AST_NODE_BINARY: %s\n", operator);
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
            printf("AST_NODE_UNARY\n");
            printNode(nodeUnary->operand, nesting + 1);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            printNestingString(nesting);
            printf("AST_NODE_VARIABLE: %.*s\n", nodeVariable->length, nodeVariable->name);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            printNestingString(nesting);
            printf("AST_NODE_ASSIGNMENT: ");
            printf("%.*s\n", nodeAssignment->length, nodeAssignment->name);
            printNode(nodeAssignment->value, nesting + 1);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            printf("AST_NODE_STATEMENTS\n");
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                printNode((AstNode*) nodeStatements->statements.values[i], nesting + 1);
            }
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            printNestingString(nesting);
            printf("AST_NODE_FUNCTION\n");
            PointerArray* statements = &nodeFunction->statements->statements;
            for (int i = 0; i < statements->count; i++) {
                printNode((AstNode*) statements->values[i], nesting + 1);
            }
            // TODO: print parameters and such
            
            break;
        }
        
        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            printNestingString(nesting);
            printf("AST_NODE_CALL\n");
            printNode((AstNode*) nodeCall->callTarget, nesting + 1);
            // TODO: print parameters and such
            
            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
			printNestingString(nesting);
			printf("AST_NODE_EXPR_STATEMENT\n");
			printNode((AstNode*) nodeExprStatement->expression, nesting + 1);
			break;
		}

        case AST_NODE_RETURN: {
			AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
			printNestingString(nesting);
			printf("AST_NODE_RETURN\n");
			printNode((AstNode*) nodeReturn->expression, nesting + 1);
			break;
		}
    }
    
//    printf("\n");
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
        
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            const char* operator = NULL;
            switch (nodeBinary->operator) {
                case TOKEN_PLUS: operator = "+"; break;
                case TOKEN_MINUS: operator = "-"; break;
                case TOKEN_STAR: operator = "*"; break;
                case TOKEN_SLASH: operator = "/"; break;
                default: operator = "Unrecognized";
            }
            
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
            deallocate(nodeFunction, sizeof(AstNodeFunction), deallocationString);
            // TODO: free parameters and such
            
            break;
        }
        
        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;
            freeNode((AstNode*) nodeCall->callTarget, nesting + 1);
            deallocate(nodeCall, sizeof(AstNodeCall), deallocationString);
            // TODO: free parameters and such
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
    }
}

void freeTree(AstNode* node) {
    DEBUG_PRINT("Freeing AST");
    freeNode(node, 0);
}

AstNodeStatements* newAstNodeStatements() {
    AstNodeStatements* node = ALLOCATE_AST_NODE(AstNodeStatements, AST_NODE_STATEMENTS);
    initPointerArray(&node->statements);
    return node;
}

AstNodeExprStatement* newAstNodeExprStatement(AstNode* expression) {
	AstNodeExprStatement* node = ALLOCATE_AST_NODE(AstNodeExprStatement, AST_NODE_EXPR_STATEMENT);
	node->expression = expression;
	return node;
}

AstNodeReturn* newAstNodeReturn(AstNode* expression) {
	AstNodeReturn* node = ALLOCATE_AST_NODE(AstNodeReturn, AST_NODE_RETURN);
	node->expression = expression;
	return node;
}

AstNode* allocateAstNode(AstNodeType type, size_t size) {
    AstNode* node = allocate(size, AST_NODE_TYPE_NAMES[type]);
    node->type = type;
    return node;
}
