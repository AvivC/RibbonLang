#include "utils.h"
//#include <windows.h>
//#include <dbghelp.h>

uint16_t twoBytesToShort(uint8_t a, uint8_t b) {
	return (a << 8) + b;
}

void short_to_two_bytes(uint16_t num, uint8_t* bytes_out) {
	bytes_out[0] = (num >> 8) & 0xFF;
	bytes_out[1] = num & 0xFF;
}

IMPLEMENT_DYNAMIC_ARRAY(size_t, IntegerArray, integer_array)

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
