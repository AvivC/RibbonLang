#include "string.h"
#include "compiler.h"
#include "parser.h"
#include "plane_object.h"
#include "ast.h"
#include "bytecode.h"
#include "value.h"
#include "memory.h"
#include "plane_utils.h"

static void emit_byte(Bytecode* chunk, uint8_t byte) {
	bytecode_write(chunk, byte);
}

static void emit_two_bytes(Bytecode* chunk, uint8_t byte1, uint8_t byte2) {
	emit_byte(chunk, byte1);
	emit_byte(chunk, byte2);
}

static void emit_short_as_two_bytes(Bytecode* chunk, uint16_t number) {
	uint8_t bytes[2];
	short_to_two_bytes(number, bytes);
	emit_byte(chunk, bytes[0]);
	emit_byte(chunk, bytes[1]);
}

static int emit_constant_operand(Bytecode* chunk, Value constant) {
	int constant_index = bytecode_add_constant(chunk, &constant);

	emit_short_as_two_bytes(chunk, constant_index);
	// emit_byte(chunk, constant_index);
	return constant_index;
}

static void emit_opcode_with_constant_operand(Bytecode* chunk, OP_CODE instruction, Value constant) {
	// emit_two_bytes(chunk, instruction, bytecode_add_constant(chunk, &constant));
	emit_byte(chunk, instruction);
	emit_constant_operand(chunk, constant);
}

static size_t emit_opcode_with_short_placeholder(Bytecode* chunk, OP_CODE opcode) {
	emit_byte(chunk, opcode);
	size_t placeholder_offset = chunk->count;
	emit_two_bytes(chunk, 0, 0);
	return placeholder_offset;
}

static void backpatch_placeholder(Bytecode* chunk, size_t placeholder_offset, size_t address) {
	bytecode_set(chunk, placeholder_offset, (address >> 8) & 0xFF);
	bytecode_set(chunk, placeholder_offset + 1, (address) & 0xFF);
}

static void backpatch_placeholder_with_current_address(Bytecode* chunk, size_t placeholder_offset) {
	backpatch_placeholder(chunk, placeholder_offset, chunk->count);
}

static void compile_tree(AstNode* node, Bytecode* bytecode) {
    AstNodeType node_type = node->type;
    
    switch (node_type) {
        case AST_NODE_BINARY: {
            AstNodeBinary* node_binary = (AstNodeBinary*) node;
            
            compile_tree(node_binary->left_operand, bytecode);
            compile_tree(node_binary->right_operand, bytecode);
            
            ScannerTokenType operator = node_binary->operator;

            // TODO: Make sure we have tests for each binary operation
            switch (operator) {
                case TOKEN_PLUS: emit_byte(bytecode, OP_ADD); break;
                case TOKEN_MINUS: emit_byte(bytecode, OP_SUBTRACT); break;
                case TOKEN_STAR: emit_byte(bytecode, OP_MULTIPLY); break;
                case TOKEN_SLASH: emit_byte(bytecode, OP_DIVIDE); break;
                case TOKEN_GREATER_THAN: emit_byte(bytecode, OP_GREATER_THAN); break;
                case TOKEN_LESS_THAN: emit_byte(bytecode, OP_LESS_THAN); break;
                case TOKEN_GREATER_EQUAL: emit_byte(bytecode, OP_GREATER_EQUAL); break;
                case TOKEN_LESS_EQUAL: emit_byte(bytecode, OP_LESS_EQUAL); break;
                case TOKEN_EQUAL_EQUAL: emit_byte(bytecode, OP_EQUAL); break;
                case TOKEN_BANG_EQUAL: emit_two_bytes(bytecode, OP_EQUAL, OP_NEGATE); break;
                default: FAIL("Unrecognized operator type: %d", operator); break;
            }
            
            break;
        }
        
        case AST_NODE_CONSTANT: {
            AstNodeConstant* node_constant = (AstNodeConstant*) node;
            emit_opcode_with_constant_operand(bytecode, OP_CONSTANT, node_constant->value);
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* node_unary = (AstNodeUnary*) node;
            compile_tree(node_unary->operand, bytecode);
            emit_byte(bytecode, OP_NEGATE);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* node_variable = (AstNodeVariable*) node;
            
            Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_variable->name, node_variable->length));
            size_t constant_index = (size_t) bytecode_add_constant(bytecode, &name_constant);

            integer_array_write(&bytecode->referenced_names_indices, &constant_index);
            
			emit_byte(bytecode, OP_LOAD_VARIABLE);
            emit_short_as_two_bytes(bytecode, constant_index);

            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* node_assignment = (AstNodeAssignment*) node;
            
            compile_tree(node_assignment->value, bytecode);

			Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_assignment->name, node_assignment->length));
			size_t constant_index = (size_t) bytecode_add_constant(bytecode, &name_constant);

			integer_array_write(&bytecode->assigned_names_indices, &constant_index);
			// emit_two_bytes(bytecode, OP_SET_VARIABLE, constant_index);
			emit_byte(bytecode, OP_SET_VARIABLE);
            emit_short_as_two_bytes(bytecode, constant_index);

            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* node_statements = (AstNodeStatements*) node;
            for (int i = 0; i < node_statements->statements.count; i++) {
                DEBUG_PRINT("Compiling statement: index %d out of %d\n", i, node_statements->statements.count);
                compile_tree((AstNode*) node_statements->statements.values[i], bytecode);
            }
            
            break;
        }
        
        case AST_NODE_FUNCTION: {
            AstNodeFunction* node_function = (AstNodeFunction*) node;
            
            Bytecode func_bytecode;
            bytecode_init(&func_bytecode);

            compiler_compile((AstNode*) node_function->statements, &func_bytecode);

            IntegerArray func_referenced_names_indices = func_bytecode.referenced_names_indices;
            for (int i = 0; i < func_referenced_names_indices.count; i++) {
            	Value referenced_name = func_bytecode.constants.values[func_referenced_names_indices.values[i]];
            	size_t constant_index = (size_t) bytecode_add_constant(bytecode, &referenced_name);
            	integer_array_write(&bytecode->referenced_names_indices, &constant_index);
			}

            Value obj_code_constant = MAKE_VALUE_OBJECT(object_code_new(func_bytecode));
            emit_opcode_with_constant_operand(bytecode, OP_MAKE_FUNCTION, obj_code_constant);

            int num_params = node_function->parameters.count;

			assert (num_params >= 0 && num_params < 65535);

            emit_short_as_two_bytes(bytecode, num_params);

            for (int i = 0; i < num_params; i++) {
            	Value param_value = node_function->parameters.values[i];
            	ASSERT_VALUE_TYPE(param_value, VALUE_RAW_STRING);
            	RawString param_raw_string = param_value.as.raw_string;
				ObjectString* param_as_object_string = object_string_copy(param_raw_string.data, param_raw_string.length);
				emit_constant_operand(bytecode, MAKE_VALUE_OBJECT(param_as_object_string));
			}

            break;
        }

		case AST_NODE_CLASS: {
			AstNodeClass* node_class = (AstNodeClass*) node;

			Bytecode body_bytecode;
            bytecode_init(&body_bytecode);

            compiler_compile((AstNode*) node_class->body, &body_bytecode);

            IntegerArray refd_names_indices = body_bytecode.referenced_names_indices;
            for (int i = 0; i < refd_names_indices.count; i++) {
            	Value referenced_name = body_bytecode.constants.values[refd_names_indices.values[i]];
            	size_t constant_index = (size_t) bytecode_add_constant(bytecode, &referenced_name);
            	integer_array_write(&bytecode->referenced_names_indices, &constant_index);
			}

            Value obj_code_constant = MAKE_VALUE_OBJECT(object_code_new(body_bytecode));
            emit_opcode_with_constant_operand(bytecode, OP_MAKE_CLASS, obj_code_constant);

			break;
		}

		case AST_NODE_NIL: {
			emit_byte(bytecode, OP_NIL);
			break;
		}
        
        case AST_NODE_CALL: {
            AstNodeCall* node_call = (AstNodeCall*) node;

			if (node_call->arguments.count > 255) {
				FAIL("Calling a function with more than 255 arguments is not supported.");
			}

            for (int i = node_call->arguments.count - 1; i >= 0; i--) {
            	AstNode* argument = node_call->arguments.values[i];
				compile_tree(argument, bytecode);
			}

            compile_tree(node_call->target, bytecode);
            emit_two_bytes(bytecode, OP_CALL, node_call->arguments.count);

            break;
        }

        case AST_NODE_KEY_ACCESS: {
        	AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;

			compile_tree(node_key_access->key, bytecode);
            compile_tree(node_key_access->subject, bytecode);
            emit_byte(bytecode, OP_ACCESS_KEY);

            break;
        }

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;

			compile_tree(node_key_assignment->value, bytecode);
			compile_tree(node_key_assignment->key, bytecode);
            compile_tree(node_key_assignment->subject, bytecode);
            emit_byte(bytecode, OP_SET_KEY);

        	break;
        }

        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;

			if (node_table->pairs.count > 255) {
				FAIL("Table initializer can't have more than 255 entries. You can add additional entires after the initializer.");
			}

			for (int i = 0; i < node_table->pairs.count; i++) {
				AstNodesKeyValuePair pair = node_table->pairs.values[i];
				compile_tree(pair.value, bytecode);
				compile_tree(pair.key, bytecode);
			}

			emit_two_bytes(bytecode, OP_MAKE_TABLE, node_table->pairs.count);

        	break;
        }

        case AST_NODE_IMPORT: {
        	AstNodeImport* node_import = (AstNodeImport*) node;

        	Value module_name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_import->name, node_import->name_length));
        	emit_opcode_with_constant_operand(bytecode, OP_IMPORT, module_name_constant);

        	break;
        }

        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;

            compile_tree(node_attr->object, bytecode);

            Value attr_name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr->name, node_attr->length));
            emit_opcode_with_constant_operand(bytecode, OP_GET_ATTRIBUTE, attr_name_constant);

            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
            AstNodeAttributeAssignment* node_attr_assignment = (AstNodeAttributeAssignment*) node;

            compile_tree(node_attr_assignment->value, bytecode);
            compile_tree(node_attr_assignment->object, bytecode);

            Value attr_name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr_assignment->name, node_attr_assignment->length));
            emit_opcode_with_constant_operand(bytecode, OP_SET_ATTRIBUTE, attr_name_constant);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
        	compile_tree(nodeExprStatement->expression, bytecode);
        	emit_byte(bytecode, OP_POP);
        	break;
        }

        case AST_NODE_RETURN: {
        	AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
        	compile_tree(nodeReturn->expression, bytecode);
        	emit_byte(bytecode, OP_RETURN);
        	break;
        }

        case AST_NODE_IF: {
        	AstNodeIf* node_if = (AstNodeIf*) node;

        	IntegerArray jump_placeholder_offsets;
        	integer_array_init(&jump_placeholder_offsets);

        	compile_tree(node_if->condition, bytecode);
        	size_t if_condition_jump_address_offset = emit_opcode_with_short_placeholder(bytecode, OP_JUMP_IF_FALSE);

        	compile_tree((AstNode*) node_if->body, bytecode);

        	size_t jump_to_end_address_offset = emit_opcode_with_short_placeholder(bytecode, OP_JUMP);
			integer_array_write(&jump_placeholder_offsets, &jump_to_end_address_offset);

        	backpatch_placeholder_with_current_address(bytecode, if_condition_jump_address_offset);

        	for (int i = 0; i < node_if->elsif_clauses.count; i += 2) {
				AstNode* condition = node_if->elsif_clauses.values[i];
				AstNodeStatements* body = node_if->elsif_clauses.values[i+1];

				compile_tree(condition, bytecode);
				if_condition_jump_address_offset = emit_opcode_with_short_placeholder(bytecode, OP_JUMP_IF_FALSE);
				compile_tree((AstNode*) body, bytecode);

				jump_to_end_address_offset = emit_opcode_with_short_placeholder(bytecode, OP_JUMP);
				integer_array_write(&jump_placeholder_offsets, &jump_to_end_address_offset);

				backpatch_placeholder_with_current_address(bytecode, if_condition_jump_address_offset);
			}

        	if (node_if->else_body != NULL) {
        		compile_tree((AstNode*) node_if->else_body, bytecode);
        	}

        	size_t end_address = bytecode->count;
        	for (int i = 0; i < jump_placeholder_offsets.count; i++) {
				size_t placeholder_offset = jump_placeholder_offsets.values[i];
				backpatch_placeholder(bytecode, placeholder_offset, end_address);
			}

        	integer_array_free(&jump_placeholder_offsets);

        	break;
        }

        case AST_NODE_WHILE: {
			AstNodeWhile* node_while = (AstNodeWhile*) node;

			int before_condition = bytecode->count;
			compile_tree(node_while->condition, bytecode);

			emit_byte(bytecode, OP_JUMP_IF_FALSE);
			size_t placeholderOffset = bytecode->count;
			emit_two_bytes(bytecode, 0, 0);

			compile_tree((AstNode*) node_while->body, bytecode);

			emit_byte(bytecode, OP_JUMP);
			uint8_t before_condition_addr_bytes[2];
			short_to_two_bytes(before_condition, before_condition_addr_bytes);
			emit_two_bytes(bytecode, before_condition_addr_bytes[0], before_condition_addr_bytes[1]);

			bytecode_set(bytecode, placeholderOffset, (bytecode->count >> 8) & 0xFF);
			bytecode_set(bytecode, placeholderOffset + 1, (bytecode->count) & 0xFF);

			break;
		}

        case AST_NODE_AND: {
        	AstNodeAnd* node_and = (AstNodeAnd*) node;
        	compile_tree((AstNode*) node_and->left, bytecode);
        	compile_tree((AstNode*) node_and->right, bytecode);
        	emit_byte(bytecode, OP_AND);

        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* node_or = (AstNodeOr*) node;
        	compile_tree((AstNode*) node_or->left, bytecode);
        	compile_tree((AstNode*) node_or->right, bytecode);
        	emit_byte(bytecode, OP_OR);

        	break;
        }

        case AST_NODE_STRING: {
        	AstNodeString* node_string = (AstNodeString*) node;
        	Value string_constant = MAKE_VALUE_OBJECT(object_string_copy(node_string->string, node_string->length));
        	emit_opcode_with_constant_operand(bytecode, OP_MAKE_STRING, string_constant);

        	break;
        }
    }
}

/* Compile a program or a function */
void compiler_compile(AstNode* node, Bytecode* bytecode) {
    compile_tree(node, bytecode);
    emit_two_bytes(bytecode, OP_NIL, OP_RETURN);
}
