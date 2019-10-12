#include "string.h"
#include "compiler.h"
#include "parser.h"
#include "object.h"
#include "ast.h"
#include "bytecode.h"
#include "value.h"
#include "memory.h"
#include "utils.h"

static void emit_byte(Bytecode* chunk, uint8_t byte) {
	bytecode_write(chunk, byte);
}

static void emit_two_bytes(Bytecode* chunk, uint8_t byte1, uint8_t byte2) {
	bytecode_write(chunk, byte1);
	bytecode_write(chunk, byte2);
}

static void emit_opcode_with_constant_operand(Bytecode* chunk, OP_CODE instruction, Value constant) {
	emit_two_bytes(chunk, instruction, bytecode_add_constant(chunk, &constant));
}

static void emit_constant(Bytecode* chunk, Value constant) {
	emit_byte(chunk, bytecode_add_constant(chunk, &constant));
}

static void emit_short_as_two_bytes(Bytecode* chunk, uint16_t number) {
	uint8_t bytes[2];
	short_to_two_bytes(number, bytes);
	emit_byte(chunk, bytes[0]);
	emit_byte(chunk, bytes[1]);
}

static size_t emit_opcode_with_short_placeholder(Bytecode* chunk, OP_CODE opcode) {
	bytecode_write(chunk, opcode);
	size_t placeholder_offset = chunk->count;
	bytecode_write(chunk, 0);
	bytecode_write(chunk, 0);
	return placeholder_offset;
}

static void backpatch_placeholder(Bytecode* chunk, size_t placeholder_offset, size_t address) {
	bytecode_set(chunk, placeholder_offset, (address >> 8) & 0xFF);
	bytecode_set(chunk, placeholder_offset + 1, (address) & 0xFF);
}

static void backpatch_placeholder_with_current_address(Bytecode* chunk, size_t placeholder_offset) {
	backpatch_placeholder(chunk, placeholder_offset, chunk->count);
}

static void compileTree(AstNode* node, Bytecode* chunk) {
    AstNodeType nodeType = node->type;
    
    switch (nodeType) {
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            ScannerTokenType operator = nodeBinary->operator;
            AstNode* leftOperand = nodeBinary->left_operand;
            AstNode* rightOperand = nodeBinary->right_operand;
            
            compileTree(leftOperand, chunk);
            compileTree(rightOperand, chunk);
            
            switch (operator) {
                case TOKEN_PLUS: bytecode_write(chunk, OP_ADD); break;
                case TOKEN_MINUS: bytecode_write(chunk, OP_SUBTRACT); break;
                case TOKEN_STAR: bytecode_write(chunk, OP_MULTIPLY); break;
                case TOKEN_SLASH: bytecode_write(chunk, OP_DIVIDE); break;
                case TOKEN_GREATER_THAN: bytecode_write(chunk, OP_GREATER_THAN); break;
                case TOKEN_LESS_THAN: bytecode_write(chunk, OP_LESS_THAN); break;
                case TOKEN_GREATER_EQUAL: bytecode_write(chunk, OP_GREATER_EQUAL); break;
                case TOKEN_LESS_EQUAL: bytecode_write(chunk, OP_LESS_EQUAL); break;
                case TOKEN_EQUAL_EQUAL: bytecode_write(chunk, OP_EQUAL); break;
                case TOKEN_BANG_EQUAL: bytecode_write(chunk, OP_EQUAL); bytecode_write(chunk, OP_NEGATE); break;
                default: FAIL("Weird operator type"); break;
            }
            
            break;
        }
        
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            
            int constantIndex = bytecode_add_constant(chunk, &nodeConstant->value);
            
            bytecode_write(chunk, OP_CONSTANT);
            bytecode_write(chunk, (uint8_t) constantIndex);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* nodeUnary = (AstNodeUnary*) node;
            compileTree(nodeUnary->operand, chunk);
            bytecode_write(chunk, OP_NEGATE);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* nodeVariable = (AstNodeVariable*) node;
            
            // TODO: Why do we sometimes use ObjectString* constants and sometimes RawString constants?
            Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(nodeVariable->name, nodeVariable->length));
            int constantIndex = bytecode_add_constant(chunk, &name_constant);

            integer_array_write(&chunk->referenced_names_indices, (size_t*) &constantIndex);
            
            bytecode_write(chunk, OP_LOAD_VARIABLE);
            bytecode_write(chunk, constantIndex);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* nodeAssignment = (AstNodeAssignment*) node;
            
            compileTree(nodeAssignment->value, chunk);
            
			Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(nodeAssignment->name, nodeAssignment->length));
			int constantIndex = bytecode_add_constant(chunk, &name_constant);

            bytecode_write(chunk, OP_SET_VARIABLE);
            bytecode_write(chunk, constantIndex);
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
            
            Bytecode function_chunk;
            bytecode_init(&function_chunk);
            compile((AstNode*) node_function->statements, &function_chunk);

            Value obj_code_constant = MAKE_VALUE_OBJECT(object_code_new(function_chunk));

            emit_opcode_with_constant_operand(chunk, OP_MAKE_FUNCTION, obj_code_constant);

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

            compileTree(nodeCall->target, chunk);
            bytecode_write(chunk, OP_CALL);
			bytecode_write(chunk, nodeCall->arguments.count);

            break;
        }

        case AST_NODE_KEY_ACCESS: {
        	AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;

			compileTree(node_key_access->key, chunk);
            compileTree(node_key_access->subject, chunk);
            bytecode_write(chunk, OP_ACCESS_KEY);

            break;
        }

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;

			compileTree(node_key_assignment->value, chunk);
			compileTree(node_key_assignment->key, chunk);
            compileTree(node_key_assignment->subject, chunk);
            bytecode_write(chunk, OP_SET_KEY);

        	break;
        }

        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;

        	if (node_table->pairs.count > 255) {
        		FAIL("Currently not supporting table literals with more than 255 entries.");
        	}

			for (int i = 0; i < node_table->pairs.count; i++) {
				AstNodesKeyValuePair pair = node_table->pairs.values[i];
				compileTree(pair.value, chunk);
				compileTree(pair.key, chunk);
			}

            bytecode_write(chunk, OP_MAKE_TABLE);
			bytecode_write(chunk, node_table->pairs.count);

        	break;
        }

        case AST_NODE_IMPORT: {
        	AstNodeImport* node_import = (AstNodeImport*) node;

        	// TODO: Not sure when the constant should be an ObjectString and when simply a RawString. Currently improvising.
        	Value module_name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_import->name, node_import->name_length));
        	emit_opcode_with_constant_operand(chunk, OP_IMPORT, module_name_constant);

        	break;
        }

        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;

            compileTree(node_attr->object, chunk);

            Value constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr->name, node_attr->length));
            int constant_index = bytecode_add_constant(chunk, &constant);

            bytecode_write(chunk, OP_GET_ATTRIBUTE);
            bytecode_write(chunk, constant_index);

            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
            AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;

            compileTree(node_attr->value, chunk);
            compileTree(node_attr->object, chunk);

            Value constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr->name, node_attr->length));
            int constant_index = bytecode_add_constant(chunk, &constant);

            bytecode_write(chunk, OP_SET_ATTRIBUTE);
            bytecode_write(chunk, constant_index);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
        	compileTree(nodeExprStatement->expression, chunk);
        	bytecode_write(chunk, OP_POP);
        	break;
        }

        case AST_NODE_RETURN: {
        	AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
        	compileTree(nodeReturn->expression, chunk);
        	bytecode_write(chunk, OP_RETURN);
        	break;
        }

        case AST_NODE_IF: {
        	AstNodeIf* node_if = (AstNodeIf*) node;

        	IntegerArray jump_placeholder_offsets;
        	integer_array_init(&jump_placeholder_offsets);

        	compileTree(node_if->condition, chunk);
        	size_t if_condition_jump_address_offset = emit_opcode_with_short_placeholder(chunk, OP_JUMP_IF_FALSE);

        	compileTree((AstNode*) node_if->body, chunk);

        	size_t jump_to_end_address_offset = emit_opcode_with_short_placeholder(chunk, OP_JUMP);
			integer_array_write(&jump_placeholder_offsets, &jump_to_end_address_offset);

        	backpatch_placeholder_with_current_address(chunk, if_condition_jump_address_offset);

        	for (int i = 0; i < node_if->elsif_clauses.count; i += 2) {
				AstNode* condition = node_if->elsif_clauses.values[i];
				AstNodeStatements* body = node_if->elsif_clauses.values[i+1];

				compileTree(condition, chunk);
				if_condition_jump_address_offset = emit_opcode_with_short_placeholder(chunk, OP_JUMP_IF_FALSE);
				compileTree((AstNode*) body, chunk);

				jump_to_end_address_offset = emit_opcode_with_short_placeholder(chunk, OP_JUMP);
				integer_array_write(&jump_placeholder_offsets, &jump_to_end_address_offset);

				backpatch_placeholder_with_current_address(chunk, if_condition_jump_address_offset);
			}

        	if (node_if->else_body != NULL) {
        		compileTree((AstNode*) node_if->else_body, chunk);
        	}

        	size_t end_address = chunk->count;
        	for (int i = 0; i < jump_placeholder_offsets.count; i++) {
				size_t placeholder_offset = jump_placeholder_offsets.values[i];
				backpatch_placeholder(chunk, placeholder_offset, end_address);
			}

        	integer_array_free(&jump_placeholder_offsets);

        	break;
        }

        case AST_NODE_WHILE: {
			AstNodeWhile* nodeWhile = (AstNodeWhile*) node;

			int before_condition = chunk->count;
			compileTree(nodeWhile->condition, chunk);

			bytecode_write(chunk, OP_JUMP_IF_FALSE);
			size_t placeholderOffset = chunk->count;
			bytecode_write(chunk, 0);
			bytecode_write(chunk, 0);

			compileTree((AstNode*) nodeWhile->body, chunk);

			bytecode_write(chunk, OP_JUMP);
			uint8_t before_condition_addr_bytes[2];
			short_to_two_bytes(before_condition, before_condition_addr_bytes);
			bytecode_write(chunk, before_condition_addr_bytes[0]);
			bytecode_write(chunk, before_condition_addr_bytes[1]);

			bytecode_set(chunk, placeholderOffset, (chunk->count >> 8) & 0xFF);
			bytecode_set(chunk, placeholderOffset + 1, (chunk->count) & 0xFF);

			break;
		}

        case AST_NODE_AND: {
        	AstNodeAnd* node_and = (AstNodeAnd*) node;
        	compileTree((AstNode*) node_and->left, chunk);
        	compileTree((AstNode*) node_and->right, chunk);
        	bytecode_write(chunk, OP_AND);

        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* node_or = (AstNodeOr*) node;
        	compileTree((AstNode*) node_or->left, chunk);
        	compileTree((AstNode*) node_or->right, chunk);
        	bytecode_write(chunk, OP_OR);

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
void compile(AstNode* node, Bytecode* chunk) {
    compileTree(node, chunk);

    // TODO: Ugly patch, and also sometimes unnecessary. Solve this in a different way.
    bytecode_write(chunk, OP_NIL);
    bytecode_write(chunk, OP_RETURN);
}
