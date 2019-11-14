#include <string.h>

#include "utils.h"
#include "memory.h"
//#include <windows.h>
//#include <dbghelp.h>

uint16_t two_bytes_to_short(uint8_t a, uint8_t b) {
	return (a << 8) + b;
}

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out) {
	bytes_out[0] = (num >> 8) & 0xFF;
	bytes_out[1] = num & 0xFF;
}

char* copy_cstring(const char* string, int length, const char* what) {
	// argument length should not include the null-terminator
	DEBUG_OBJECTS_PRINT("Allocating string buffer '%.*s' of length %d.", length, string, length);

    char* chars = allocate(sizeof(char) * length + 1, what);
    memcpy(chars, string, length);
    chars[length] = '\0';

    return chars;
}

char* copy_null_terminated_cstring(const char* string, const char* what) {
	return copy_cstring(string, strlen(string), what);
}

IMPLEMENT_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

IMPLEMENT_DYNAMIC_ARRAY(char, CharacterArray, character_array)

// TODO: Make this work.

//void printStack( void )
//{
//     unsigned int   i;
//     void         * stack[ 100 ];
//     unsigned short frames;
//     SYMBOL_INFO  * symbol;
//     HANDLE         process;
//
//     process = GetCurrentProcess();
//
//     SymInitialize( process, NULL, TRUE );
//
//     frames               = CaptureStackBackTrace( 0, 100, stack, NULL );
//     symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
//     symbol->MaxNameLen   = 255;
//     symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
//
//     for( i = 0; i < frames; i++ )
//     {
//         SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );
//
//         printf( "%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
//     }
//
//     free( symbol );
//}
