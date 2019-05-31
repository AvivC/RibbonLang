#ifndef plane_compiler_h
#define plane_compiler_h

#include "chunk.h"
#include "ast.h"

void compile(AstNode* node, Chunk* chunk);

#endif
