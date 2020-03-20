#include <gtk/gtk.h>

#include "builtin_module_gui.h"
#include "plane_object.h"
#include "vm.h"

/* Yep, big global variable for now, yikessss */
static bool gui_app_healthy = true;

static void show_window(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new (app);
    gtk_window_set_title(GTK_WINDOW(window), "Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);
    gtk_widget_show_all(window);

    ObjectFunction* func = user_data;
    ValueArray args;
    value_array_init(&args);
    Value out;
    InterpretResult func_exec_result = vm_call_function_directly(func, args, &out);
    if (func_exec_result != INTERPRET_SUCCESS) {
        gui_app_healthy = false;
        g_application_quit(G_APPLICATION(app));
    }
}

bool gui_window_new(ValueArray args, Value* out) {
    /* TODO: Currently assuming this function is only ever called once, deal with this later */

    ObjectFunction* func = NULL;
    if (args.count != 1 
        || (func = VALUE_AS_OBJECT(args.values[0], OBJECT_FUNCTION, ObjectFunction)) == NULL) {
        return false; /* TODO: Decent runtime error handling */
    }

    GtkApplication* app = gtk_application_new("plane.gtk.gui.app", G_APPLICATION_FLAGS_NONE);
    // g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(show_window), func);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    // printf("\nZ\n");
    g_object_unref(app);

    *out = MAKE_VALUE_NIL();

    return gui_app_healthy;
}