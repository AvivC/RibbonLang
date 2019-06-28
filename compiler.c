#include "compiler.h"
#include "parser.h"
#include "object.h"
#include "chunk.h"
#include "ast.h"
#include "value.h"

static void compileTree(AstNode* node, Chunk* chunk) {
    AstNodeType nodeType = node->type;
    
    switch (nodeType) {
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            ScannerTokenType operator = nodeBinary->operator;
            AstNode* leftOperand = nodeBinary->leftOperand;
            AstNode* rightOperand = nodeBinary->rightOperand;
            
            compileTree(leftOperand, chunk);
            compileTree(rightOperand, chunk);
            
            switch (operator) {
                case TOKEN_PLUS: writeChunk(chunk, OP_ADD); break;
                case TOKEN_MINUS: writeChunk(chunk, OP_SUBTRACT); break;
                case TOKEN_STAR: writeChunk(chunk, OP_MULTIPLY); break;
                case TOKEN_SLASH: writeChunk(chunk, OP_DIVIDE); break;
                case TOKEN_GREATER_THAN: writeChunk(chunk, OP_GREATER_THAN); break;
                case TOKEN_LESS_THAN: writeChunk(chunk, OP_LESS_THAN); break;
                case TOKEN_GREATER_EQUAL: writeChunk(chunk, OP_GREATER_EQUAL); break;
                case TOKEN_LESS_EQUAL: writeChunk(chunk, OP_LESS_EQUAL); break;
                case TOKEN_EQUAL_EQUAL: writeChunk(chunk, OP_EQUAL); break;
                case TOKEN_BANG_EQUAL: writeChunk(chunk, OP_EQUAL); writeChunk(chunk, OP_NEGATE); break;
                default: FAIL("Weird operator type"); break;
            }
            
            break;
        }
        
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            
            int constantIndex = addConstant(chunk, nodeConstant->value);
            
            writeChunk(chunk, OP_CONSTANT);
            writeChunk(chunk, (uint8_t) constantIndex);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            compileTree(nodeUnary->operand, chunk);
            writeChunk(chunk, OP_NEGATE);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            
            ObjectString* stringObj = copyString(nodeVariable->name, nodeVariable->length);
            Value nameValue = MAKE_VALUE_OBJECT(stringObj);
            int constantIndex = addConstant(chunk, nameValue);
            
            writeChunk(chunk, OP_LOAD_VARIABLE);
            writeChunk(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            
            compileTree(nodeAssignment->value, chunk);
            
            Value nameValue = MAKE_VALUE_OBJECT(copyString(nodeAssignment->name, nodeAssignment->length));
            int constantIndex = addConstant(chunk, nameValue);
            
            writeChunk(chunk, OP_SET_VARIABLE);
            writeChunk(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                DEBUG_PRINT("Compiling statement: index %d out of %d\n", i, nodeStatements->statements.count);
                compileTree((AstNode*) nodeStatements->statements.values[i], chunk);
            }
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* nodeFunction = (AstNodeFunction*) node;
            
            Chunk functionChunk;
            initChunk(&functionChunk);
            compile((AstNode*) nodeFunction->statements, &functionChunk); // calling compile() and not compileTree(), because it ends with OP_RETURN

            ObjectString** parameters = (ObjectString**) pointerArrayToPlainArray(&nodeFunction->parameters, "Parameters list");
            ObjectFunction* objFunc = newUserObjectFunction(functionChunk, parameters, nodeFunction->parameters.count);
            Value objFuncConstant = MAKE_VALUE_OBJECT(objFunc);
            int constantIndex = addConstant(chunk, objFuncConstant);
            
            writeChunk(chunk, OP_CONSTANT);
            writeChunk(chunk, (uint8_t) constantIndex);
            
            break;
        }
        
        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;

            for (int i = nodeCall->arguments.count - 1; i >= 0; i--) {
            	AstNode* argument = nodeCall->arguments.values[i];
				compileTree(argument, chunk);
			}

            compileTree(nodeCall->callTarget, chunk);
            writeChunk(chunk, OP_CALL);
			writeChunk(chunk, nodeCall->arguments.count);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
        	compileTree(nodeExprStatement->expression, chunk);
        	writeChunk(chunk, OP_POP);
        	break;
        }

        case AST_NODE_RETURN: {
        	AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
        	compileTree(nodeReturn->expression, chunk);
        	writeChunk(chunk, OP_RETURN);
        	break;
        }

        case AST_NODE_IF: {
        	AstNodeIf* nodeIf = (AstNodeIf*) node;
        	compileTree(nodeIf->condition, chunk);

        	writeChunk(chunk, OP_JUMP_IF_FALSE);
        	size_t placeholderOffset = chunk->count;
        	writeChunk(chunk, 0);
        	writeChunk(chunk, 0);

        	compileTree((AstNode*) nodeIf->body, chunk);

        	writeChunk(chunk, OP_JUMP);
			size_t skipElseClausesOffset = chunk->count;
			writeChunk(chunk, 0);
			writeChunk(chunk, 0);

        	for (int i = 0; i < nodeIf->elsifClauses.count; i += 2) {
        		AstNode* condition = nodeIf->elsifClauses.values[i];
        		AstNodeStatements* body = nodeIf->elsifClauses.values[i+1];

        		setChunk(chunk, placeholderOffset, (chunk->count >> 8) & 0xFF);
				setChunk(chunk, placeholderOffset + 1, (chunk->count) & 0xFF);

        		compileTree(condition, chunk);

        		writeChunk(chunk, OP_JUMP_IF_FALSE);
        		placeholderOffset = chunk->count;
				writeChunk(chunk, 0);
				writeChunk(chunk, 0);

				compileTree((AstNode*) body, chunk);
			}

        	bool haveElse = nodeIf->elseBody != NULL;
        	if (haveElse) {
        		writeChunk(chunk, OP_JUMP);
				size_t elsePlaceholderOffset = chunk->count;
				writeChunk(chunk, 0);
				writeChunk(chunk, 0);

				setChunk(chunk, placeholderOffset, (chunk->count >> 8) & 0xFF);
				setChunk(chunk, placeholderOffset + 1, (chunk->count) & 0xFF);

				compileTree((AstNode*) nodeIf->elseBody, chunk);

	        	setChunk(chunk, elsePlaceholderOffset, (chunk->count >> 8) & 0xFF);
	        	setChunk(chunk, elsePlaceholderOffset + 1, (chunk->count) & 0xFF);

        	} else {
        		setChunk(chunk, placeholderOffset, (chunk->count >> 8) & 0xFF);
        		setChunk(chunk, placeholderOffset + 1, (chunk->count) & 0xFF);
        	}

        	setChunk(chunk, skipElseClausesOffset, (chunk->count >> 8) & 0xFF);
			setChunk(chunk, skipElseClausesOffset + 1, (chunk->count) & 0xFF);
        	break;
        }
    }
}

/* Compile a program or a function */
void compile(AstNode* node, Chunk* chunk) {
    compileTree(node, chunk);
    writeChunk(chunk, OP_NIL);
    writeChunk(chunk, OP_RETURN);
}
