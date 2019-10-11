#ifndef plane_compiler_h
#define plane_compiler_h

#include "ast.h"
#include "bytecode.h"

void compile(AstNode* node, Bytecode* chunk);

#endif
