#ifndef ribbon_parser_h
#define ribbon_parser_h

#include "ast.h"
#include "bytecode.h"

AstNode* parser_parse(const char* source, const char* file_path);

#endif