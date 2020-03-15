#include <gtk/gtk.h>

#include "builtin_module_gui.h"
#include "plane_object.h"

static void show_window(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new (app);
    gtk_window_set_title(GTK_WINDOW(window), "Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);
    gtk_widget_show_all(window);
}

bool gui_window_new(ValueArray args, Value* out) {
    ObjectFunction* func = NULL;
    if (args.count != 1 
        || (func = VALUE_AS_OBJECT(args.values[0], OBJECT_FUNCTION, ObjectFunction)) == NULL) {
        return false; /* TODO: Decent runtime error handling */
    }

    GtkApplication* app = gtk_application_new("plane.gtk.gui.app", G_APPLICATION_FLAGS_NONE);
    // g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(show_window), func);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    *out = MAKE_VALUE_NIL();
    return true;
}