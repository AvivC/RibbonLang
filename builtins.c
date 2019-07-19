#include <stdio.h>

#include "builtins.h"
#include "value.h"

bool builtinPrint(ValueArray args, Value* out) {
	printValue(args.values[0]);
	printf("\n");

	*out = MAKE_VALUE_NIL();
	return true;
}
