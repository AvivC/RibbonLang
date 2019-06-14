#include <stdio.h>

#include "builtins.h"
#include "value.h"

Value builtinPrint(ValueArray args) {
	printValue(args.values[0]);
	printf("\n");
	return MAKE_VALUE_NIL();
}
