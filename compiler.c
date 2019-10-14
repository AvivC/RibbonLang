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

static void compile_tree(AstNode* node, Bytecode* bytecode) {
    AstNodeType nodeType = node->type;
    
    switch (nodeType) {
        case AST_NODE_BINARY: {
            AstNodeBinary* nodeBinary = (AstNodeBinary*) node;
            
            ScannerTokenType operator = nodeBinary->operator;
            AstNode* leftOperand = nodeBinary->left_operand;
            AstNode* rightOperand = nodeBinary->right_operand;
            
            compile_tree(leftOperand, bytecode);
            compile_tree(rightOperand, bytecode);
            
            switch (operator) {
                case TOKEN_PLUS: bytecode_write(bytecode, OP_ADD); break;
                case TOKEN_MINUS: bytecode_write(bytecode, OP_SUBTRACT); break;
                case TOKEN_STAR: bytecode_write(bytecode, OP_MULTIPLY); break;
                case TOKEN_SLASH: bytecode_write(bytecode, OP_DIVIDE); break;
                case TOKEN_GREATER_THAN: bytecode_write(bytecode, OP_GREATER_THAN); break;
                case TOKEN_LESS_THAN: bytecode_write(bytecode, OP_LESS_THAN); break;
                case TOKEN_GREATER_EQUAL: bytecode_write(bytecode, OP_GREATER_EQUAL); break;
                case TOKEN_LESS_EQUAL: bytecode_write(bytecode, OP_LESS_EQUAL); break;
                case TOKEN_EQUAL_EQUAL: bytecode_write(bytecode, OP_EQUAL); break;
                case TOKEN_BANG_EQUAL: bytecode_write(bytecode, OP_EQUAL); bytecode_write(bytecode, OP_NEGATE); break;
                default: FAIL("Weird operator type"); break;
            }
            
            break;
        }
        
        case AST_NODE_CONSTANT: {
            AstNodeConstant* nodeConstant = (AstNodeConstant*) node;
            
            int constantIndex = bytecode_add_constant(bytecode, &nodeConstant->value);
            
            bytecode_write(bytecode, OP_CONSTANT);
            bytecode_write(bytecode, (uint8_t) constantIndex);
            
            break;
        }
        
        case AST_NODE_UNARY: {
            AstNodeUnary* node_unary = (AstNodeUnary*) node;
            compile_tree(node_unary->operand, bytecode);
            bytecode_write(bytecode, OP_NEGATE);
            break;
        }
        
        case AST_NODE_VARIABLE: {
            AstNodeVariable* node_variable = (AstNodeVariable*) node;
            
            // TODO: Why do we sometimes use ObjectString* constants and sometimes RawString constants?
            Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_variable->name, node_variable->length));
            int constant_index = bytecode_add_constant(bytecode, &name_constant);

            integer_array_write(&bytecode->referenced_names_indices, (size_t*) &constant_index);
            
            bytecode_write(bytecode, OP_LOAD_VARIABLE);
            bytecode_write(bytecode, constant_index);
            break;
        }
        
        case AST_NODE_ASSIGNMENT: {
            AstNodeAssignment* node_assignment = (AstNodeAssignment*) node;
            
            compile_tree(node_assignment->value, bytecode);
            
			Value name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_assignment->name, node_assignment->length));
			int constantIndex = bytecode_add_constant(bytecode, &name_constant);

            bytecode_write(bytecode, OP_SET_VARIABLE);
            bytecode_write(bytecode, constantIndex);
            break;
        }
        
        case AST_NODE_STATEMENTS: {
            AstNodeStatements* nodeStatements = (AstNodeStatements*) node;
            for (int i = 0; i < nodeStatements->statements.count; i++) {
                DEBUG_PRINT("Compiling statement: index %d out of %d\n", i, nodeStatements->statements.count);
                compile_tree((AstNode*) nodeStatements->statements.values[i], bytecode);
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
            	int constant_index = bytecode_add_constant(bytecode, &referenced_name);
            	integer_array_write(&bytecode->referenced_names_indices, (size_t*) &constant_index);
			}

            Value obj_code_constant = MAKE_VALUE_OBJECT(object_code_new(func_bytecode));
            emit_opcode_with_constant_operand(bytecode, OP_MAKE_FUNCTION, obj_code_constant);

            int num_params = node_function->parameters.count;

            if (num_params > 65535) {
            	// TODO: This should probably be a runtime error, not an assertion
            	FAIL("Way too many function parameters: %d", num_params);
            }
            if (num_params < 0) {
            	FAIL("Negative number of parameters... '%d'", num_params);
            }

            emit_short_as_two_bytes(bytecode, num_params);

			for (int i = 0; i < num_params; i++) {
				emit_constant(bytecode, node_function->parameters.values[i]);
			}

            break;
        }
        
        case AST_NODE_CALL: {
            AstNodeCall* nodeCall = (AstNodeCall*) node;

            for (int i = nodeCall->arguments.count - 1; i >= 0; i--) {
            	AstNode* argument = nodeCall->arguments.values[i];
				compile_tree(argument, bytecode);
			}

            compile_tree(nodeCall->target, bytecode);
            bytecode_write(bytecode, OP_CALL);
			bytecode_write(bytecode, nodeCall->arguments.count);

            break;
        }

        case AST_NODE_KEY_ACCESS: {
        	AstNodeKeyAccess* node_key_access = (AstNodeKeyAccess*) node;

			compile_tree(node_key_access->key, bytecode);
            compile_tree(node_key_access->subject, bytecode);
            bytecode_write(bytecode, OP_ACCESS_KEY);

            break;
        }

        case AST_NODE_KEY_ASSIGNMENT: {
        	AstNodeKeyAssignment* node_key_assignment = (AstNodeKeyAssignment*) node;

			compile_tree(node_key_assignment->value, bytecode);
			compile_tree(node_key_assignment->key, bytecode);
            compile_tree(node_key_assignment->subject, bytecode);
            bytecode_write(bytecode, OP_SET_KEY);

        	break;
        }

        case AST_NODE_TABLE: {
        	AstNodeTable* node_table = (AstNodeTable*) node;

        	if (node_table->pairs.count > 255) {
        		FAIL("Currently not supporting table literals with more than 255 entries.");
        	}

			for (int i = 0; i < node_table->pairs.count; i++) {
				AstNodesKeyValuePair pair = node_table->pairs.values[i];
				compile_tree(pair.value, bytecode);
				compile_tree(pair.key, bytecode);
			}

            bytecode_write(bytecode, OP_MAKE_TABLE);
			bytecode_write(bytecode, node_table->pairs.count);

        	break;
        }

        case AST_NODE_IMPORT: {
        	AstNodeImport* node_import = (AstNodeImport*) node;

        	// TODO: Not sure when the constant should be an ObjectString and when simply a RawString. Currently improvising.
        	Value module_name_constant = MAKE_VALUE_OBJECT(object_string_copy(node_import->name, node_import->name_length));
        	emit_opcode_with_constant_operand(bytecode, OP_IMPORT, module_name_constant);
        	emit_byte(bytecode, OP_POP); // Compiler always puts a OP_NIL OP_RETURN at the end, but that's irrelevant for modules.. change this patch later.

        	break;
        }

        case AST_NODE_ATTRIBUTE: {
            AstNodeAttribute* node_attr = (AstNodeAttribute*) node;

            compile_tree(node_attr->object, bytecode);

            Value constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr->name, node_attr->length));
            int constant_index = bytecode_add_constant(bytecode, &constant);

            bytecode_write(bytecode, OP_GET_ATTRIBUTE);
            bytecode_write(bytecode, constant_index);

            break;
        }

        case AST_NODE_ATTRIBUTE_ASSIGNMENT: {
            AstNodeAttributeAssignment* node_attr = (AstNodeAttributeAssignment*) node;

            compile_tree(node_attr->value, bytecode);
            compile_tree(node_attr->object, bytecode);

            Value constant = MAKE_VALUE_OBJECT(object_string_copy(node_attr->name, node_attr->length));
            int constant_index = bytecode_add_constant(bytecode, &constant);

            bytecode_write(bytecode, OP_SET_ATTRIBUTE);
            bytecode_write(bytecode, constant_index);

            break;
        }

        case AST_NODE_EXPR_STATEMENT: {
        	AstNodeExprStatement* nodeExprStatement = (AstNodeExprStatement*) node;
        	compile_tree(nodeExprStatement->expression, bytecode);
        	bytecode_write(bytecode, OP_POP);
        	break;
        }

        case AST_NODE_RETURN: {
        	AstNodeReturn* nodeReturn = (AstNodeReturn*) node;
        	compile_tree(nodeReturn->expression, bytecode);
        	bytecode_write(bytecode, OP_RETURN);
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
			AstNodeWhile* nodeWhile = (AstNodeWhile*) node;

			int before_condition = bytecode->count;
			compile_tree(nodeWhile->condition, bytecode);

			bytecode_write(bytecode, OP_JUMP_IF_FALSE);
			size_t placeholderOffset = bytecode->count;
			bytecode_write(bytecode, 0);
			bytecode_write(bytecode, 0);

			compile_tree((AstNode*) nodeWhile->body, bytecode);

			bytecode_write(bytecode, OP_JUMP);
			uint8_t before_condition_addr_bytes[2];
			short_to_two_bytes(before_condition, before_condition_addr_bytes);
			bytecode_write(bytecode, before_condition_addr_bytes[0]);
			bytecode_write(bytecode, before_condition_addr_bytes[1]);

			bytecode_set(bytecode, placeholderOffset, (bytecode->count >> 8) & 0xFF);
			bytecode_set(bytecode, placeholderOffset + 1, (bytecode->count) & 0xFF);

			break;
		}

        case AST_NODE_AND: {
        	AstNodeAnd* node_and = (AstNodeAnd*) node;
        	compile_tree((AstNode*) node_and->left, bytecode);
        	compile_tree((AstNode*) node_and->right, bytecode);
        	bytecode_write(bytecode, OP_AND);

        	break;
        }

        case AST_NODE_OR: {
        	AstNodeOr* node_or = (AstNodeOr*) node;
        	compile_tree((AstNode*) node_or->left, bytecode);
        	compile_tree((AstNode*) node_or->right, bytecode);
        	bytecode_write(bytecode, OP_OR);

        	break;
        }

        case AST_NODE_STRING: {
        	AstNodeString* node_string = (AstNodeString*) node;

//        	Value constant = MAKE_VALUE_RAW_STRING(node_string->string, node_string->length);
        	Value constant = MAKE_VALUE_OBJECT(object_string_copy(node_string->string, node_string->length));
        	emit_opcode_with_constant_operand(bytecode, OP_MAKE_STRING, constant);
//        	printf("\n%.*s\n", ((ObjectString*) constant.as.object)->length, ((ObjectString*) constant.as.object)->chars);

        	break;
        }
    }
}

/* Compile a program or a function */
void compiler_compile(AstNode* node, Bytecode* chunk) {
    compile_tree(node, chunk);

    // TODO: Ugly patch, and also sometimes unnecessary. Solve this in a different way.
    bytecode_write(chunk, OP_NIL);
    bytecode_write(chunk, OP_RETURN);
}
