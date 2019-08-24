#include "string.h"
#include "compiler.h"
#include "parser.h"
#include "object.h"
#include "chunk.h"
#include "ast.h"
#include "value.h"
#include "memory.h"
#include "utils.h"

static void emit_byte(Chunk* chunk, uint8_t byte) {
	writeChunk(chunk, byte);
}

static void emit_two_bytes(Chunk* chunk, uint8_t byte1, uint8_t byte2) {
	writeChunk(chunk, byte1);
	writeChunk(chunk, byte2);
}

static void emit_opcode_with_constant_operand(Chunk* chunk, OP_CODE instruction, Value constant) {
	emit_two_bytes(chunk, instruction, addConstant(chunk, &constant));
}

static void emit_constant(Chunk* chunk, Value constant) {
	emit_byte(chunk, addConstant(chunk, &constant));
}

static void emit_short_as_two_bytes(Chunk* chunk, uint16_t number) {
	uint8_t bytes[2];
	short_to_two_bytes(number, bytes);
	emit_byte(chunk, bytes[0]);
	emit_byte(chunk, bytes[1]);
}

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
            
            int constantIndex = addConstant(chunk, &nodeConstant->value);
            
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
            
//            Value name_constant = MAKE_VALUE_RAW_STRING(nodeVariable->name, nodeVariable->length);
            Value name_constant = MAKE_VALUE_OBJECT(copyString(nodeVariable->name, nodeVariable->length));
            int constantIndex = addConstant(chunk, &name_constant);
            
            writeChunk(chunk, OP_LOAD_VARIABLE);
            writeChunk(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            
            compileTree(nodeAssignment->value, chunk);
            
//			Value name_constant = MAKE_VALUE_RAW_STRING(nodeAssignment->name, nodeAssignment->length);
			Value name_constant = MAKE_VALUE_OBJECT(copyString(nodeAssignment->name, nodeAssignment->length));
			int constantIndex = addConstant(chunk, &name_constant);

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
            AstNodeFunction* node_function = (AstNodeFunction*) node;
            
            Chunk function_chunk;
            initChunk(&function_chunk);
            compile((AstNode*) node_function->statements, &function_chunk);
            Value constant_chunk = MAKE_VALUE_CHUNK(function_chunk);

            emit_opcode_with_constant_operand(chunk, OP_MAKE_FUNCTION, constant_chunk);

            int num_params = node_function->parameters.count;

            if (num_params > 65535) {
            	FAIL("Way too many function parameters: %d", num_params);
            }
            if (num_params < 0) {
            	FAIL("Negative number of parameters... '%d'", num_params);
            }

            emit_short_as_two_bytes(chunk, num_params);

			for (int i = 0; i < num_params; i++) {
				emit_constant(chunk, node_function->parameters.values[i]);
			}

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

        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;

            compileTree(node_attr->object, chunk);

            Value constant = MAKE_VALUE_OBJECT(copyString(node_attr->name, node_attr->length));
            int constant_index = addConstant(chunk, &constant);

            writeChunk(chunk, OP_GET_ATTRIBUTE);
            writeChunk(chunk, constant_index);

            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
            AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;

            compileTree(node_attr->value, chunk);
            compileTree(node_attr->object, chunk);

            Value constant = MAKE_VALUE_OBJECT(copyString(node_attr->name, node_attr->length));
            int constant_index = addConstant(chunk, &constant);

            writeChunk(chunk, OP_SET_ATTRIBUTE);
            writeChunk(chunk, constant_index);

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

        case AST_NODE_WHILE: {
			AstNodeWhile* nodeWhile = (AstNodeWhile*) node;

			int before_condition = chunk->count;
			compileTree(nodeWhile->condition, chunk);

			writeChunk(chunk, OP_JUMP_IF_FALSE);
			size_t placeholderOffset = chunk->count;
			writeChunk(chunk, 0);
			writeChunk(chunk, 0);

			compileTree((AstNode*) nodeWhile->body, chunk);

			writeChunk(chunk, OP_JUMP);
			uint8_t before_condition_addr_bytes[2];
			short_to_two_bytes(before_condition, before_condition_addr_bytes);
			writeChunk(chunk, before_condition_addr_bytes[0]);
			writeChunk(chunk, before_condition_addr_bytes[1]);

			setChunk(chunk, placeholderOffset, (chunk->count >> 8) & 0xFF);
			setChunk(chunk, placeholderOffset + 1, (chunk->count) & 0xFF);

			break;
		}

        case AST_NODE_AND: {
        	AstNodeAnd* node_and = (AstNodeAnd*) node;
        	compileTree((AstNode*) node_and->left, chunk);
        	compileTree((AstNode*) node_and->right, chunk);
        	writeChunk(chunk, OP_AND);

        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* node_or = (AstNodeOr*) node;
        	compileTree((AstNode*) node_or->left, chunk);
        	compileTree((AstNode*) node_or->right, chunk);
        	writeChunk(chunk, OP_OR);

        	break;
        }

        case AST_NODE_STRING: {
        	AstNodeString* node_string = (AstNodeString*) node;

        	Value constant = MAKE_VALUE_RAW_STRING(node_string->string, node_string->length);
        	emit_opcode_with_constant_operand(chunk, OP_MAKE_STRING, constant);

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
