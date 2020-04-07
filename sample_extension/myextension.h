#include <stdbool.h>

#include <plane.h>

#ifdef MYEXTENSION_EXPORTS
    #define MYEXTENSIONAPI __declspec(dllexport)
#else
    #define MYEXTENSIONAPI __declspec(dllimport)
#endif

MYEXTENSIONAPI bool plane_module_init(PlaneApi api, ObjectModule* module);
