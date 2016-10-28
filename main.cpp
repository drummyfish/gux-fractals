#include <gtk/gtk.h>
#include <gdk/gdk.h>

GtkWidget *window;
GtkWidget *paned;
GtkWidget *paned2;

GtkWidget *button;
GtkWidget *button2;

GtkWidget *frame_menu;
GtkWidget *frame_render;
GtkWidget *frame_draw;


int main(int argc, char *argv[])
  {
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window),800,600);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    paned2 = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    button = gtk_button_new_with_label("abc");
    button2 = gtk_button_new_with_label("def");

    frame_menu = gtk_frame_new("menu");
    frame_render = gtk_frame_new("render");
    frame_draw = gtk_frame_new("draw");

    gtk_widget_set_hexpand(paned,true);
    gtk_widget_set_vexpand(paned,true);

    GdkRectangle preferred_menu_size;
    preferred_menu_size.x = 0;
    preferred_menu_size.y = 0;
    preferred_menu_size.width = 100;
    preferred_menu_size.x = 1000;

    gtk_widget_size_allocate(frame_menu,&preferred_menu_size);

    gtk_paned_add1(GTK_PANED(paned),frame_menu);
    gtk_paned_add2(GTK_PANED(paned),paned2);

    gtk_paned_add1(GTK_PANED(paned2),frame_render);
    gtk_paned_add2(GTK_PANED(paned2),frame_draw);

    gtk_paned_set_position(GTK_PANED(paned),200);
    gtk_paned_set_position(GTK_PANED(paned2),400);

    gtk_container_add(GTK_CONTAINER(window),paned);

    gtk_window_set_title(GTK_WINDOW(window),"fractal generator");
    gtk_container_set_border_width(GTK_CONTAINER(window),10);
  
    gtk_widget_show_all(window);
  
    g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),NULL);  

    gtk_main();

    return 0;
  }
