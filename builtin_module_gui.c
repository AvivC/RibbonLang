#include <gtk/gtk.h>

#include "builtin_module_gui.h"
#include "common.h"
#include "value.h"

static void show_window(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new (app);
    gtk_window_set_title(GTK_WINDOW(window), "Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);
    gtk_widget_show_all(window);
}

bool gui_window_new(ValueArray args, Value* out) {
    GtkApplication* app = gtk_application_new("plane.gtk.gui.app", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    *out = MAKE_VALUE_NIL();
    return true;
}