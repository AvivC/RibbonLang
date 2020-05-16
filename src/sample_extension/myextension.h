#include <stdbool.h>

#include <ribbon_api.h>

#ifdef MYEXTENSION_EXPORTS
    #define MYEXTENSIONAPI __declspec(dllexport)
#else
    #define MYEXTENSIONAPI __declspec(dllimport)
#endif

MYEXTENSIONAPI bool ribbon_module_init(RibbonApi api, ObjectModule* module);
