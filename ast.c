#include <stdio.h>

#include "ast.h"
#include "memory.h"

static void printNestingString(int nesting) {
    for (int i = 0; i < nesting; i++) {
        printf("    ");
    }
}

static void printNode(AstNode* node, int nesting) {
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            printNestingString(nesting);
            printf("AST_NODE_CONSTANT: %f", nodeConstant->value);
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
        
        
        default: {
            printNestingString(nesting);
            printf("Unrecognized AstNodeType.\n");
            break;
        }
    }
    
    printf("\n");
}

void printTree(AstNode* tree) {
    printNode(tree, 0);
}

static void freeNode(AstNode* node, int nesting) {
    switch (node->type) {
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            printNestingString(nesting);
            printf("Deallocating AstNodeConstant.\n");
            deallocate(nodeConstant, sizeof(AstNodeConstant));
            break;
        }
        
        case AST_NODE_BINARY: {
            printNestingString(nesting);
            printf("Starting cleanup on AstNodeBinary.\n");
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
            
            printNestingString(nesting);
            printf("Deallocating AstNodeBinary: %s.\n", operator);
            deallocate(nodeBinary, sizeof(AstNodeBinary));
            
            break;
        }
        
        default: {
            printNestingString(nesting);
            printf("Unrecognized AstNodeType.\n");
            break;
        }
    }
}

void freeTree(AstNode* node) {
    DEBUG_PRINT("Freeing AST.");
    freeNode(node, 0);
}
