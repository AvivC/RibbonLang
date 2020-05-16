#ifndef ribbon_compiler_h
#define ribbon_compiler_h

#include "ast.h"
#include "bytecode.h"

void compiler_compile(AstNode* node, Bytecode* chunk);

#endif
