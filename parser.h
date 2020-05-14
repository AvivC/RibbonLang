#ifndef plane_parser_h
#define plane_parser_h

#include "ast.h"
#include "bytecode.h"

AstNode* parser_parse(const char* source, const char* file_path);

#endif