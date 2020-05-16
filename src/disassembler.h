#ifndef ribbon_disassembler_h
#define ribbon_disassembler_h

#include "bytecode.h"
#include "common.h"

void disassembler_do_bytecode(Bytecode* chunk);
int disassembler_do_single_instruction(OP_CODE opcode, Bytecode* chunk, int offset);

#endif
