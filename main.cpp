#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <iostream>
#include <vector>

using namespace std;

GtkWidget *window;
GtkWidget *paned;
GtkWidget *paned2;
GtkWidget *box;

GtkWidget *button_render;
GtkWidget *button_clear;

GtkWidget *frame_menu;
GtkWidget *frame_render;
GtkWidget *frame_draw;

GtkWidget *draw_area;
GtkWidget *draw_area2;

typedef struct
  {
    double x1;      // normalise coordinates
    double y1;
    double x2;      // normalise coordinates
    double y2;
    bool iterate;  // if true, the segment will be iterated on
  } fractal_segment;

vector<fractal_segment> fractal_segments;  // segments the user has drawn
int draw_state = 0;                        // 0 - waiting for point 1, 1 - waiting for point 2

double tmp_x1;
double tmp_y1;
double cursor_x;
double cursor_y;

gboolean draw_callback(GtkWidget *widget,cairo_t *cr,gpointer data)
  {
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context,cr,0,0,width,height);

    cairo_arc(cr,width / 2.0, height / 2.0,MIN (width, height) / 2.0,0, 2 * G_PI);

    gtk_style_context_get_color(context,gtk_style_context_get_state(context),&color);
    gdk_cairo_set_source_rgba(cr,&color);

    cairo_fill(cr);

    return FALSE;
  }

gboolean draw_callback2(GtkWidget *widget,cairo_t *cr,gpointer data)
  {
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_set_source_rgb(cr,0,0,0);
    cairo_set_line_width (cr,2);

    for (int i = 0; i < fractal_segments.size(); i++)
      {
        fractal_segment segment = fractal_segments[i];

        cairo_move_to(cr,segment.x1 * width,segment.y1 * height);
        cairo_line_to(cr,segment.x2 * width,segment.y2 * height);
      }

    if (draw_state == 1)
      {
        cairo_move_to(cr,tmp_x1 * width,tmp_y1 * height);
        cairo_line_to(cr,cursor_x,cursor_y);
      }

    cairo_stroke(cr);

    if (fractal_segments.size() > 0)
      {
        cairo_set_source_rgb(cr,1,0,0);
        cairo_arc(cr,fractal_segments[0].x1 * width,fractal_segments[0].y1 * height,5,0,2 * G_PI);
        cairo_arc(cr,fractal_segments[fractal_segments.size() - 1].x2 * width,fractal_segments[fractal_segments.size() - 1].y2 * height,5,0,2 * G_PI);
      }

    cairo_fill(cr);
    return FALSE;
  }

gboolean mouse_motion_callback2(GtkWidget *widget,GdkEventButton *event)
  {
    if (draw_state == 1)       // waiting for second point => redraw
      {
        cursor_x = event->x;
        cursor_y = event->y;
        gtk_widget_queue_draw(draw_area2);
      }

    return FALSE;
  }

gboolean mouse_press_callback2(GtkWidget *widget,GdkEventButton *event)
  {
    guint width, height;

    width = gtk_widget_get_allocated_width(draw_area2);
    height = gtk_widget_get_allocated_height(draw_area2);

    double x, y;

    x = event->x / double(width);
    y = event->y / double(height);

    if (draw_state == 0)
      {
        tmp_x1 = x;
        tmp_y1 = y;
        draw_state = 1;
      }
    else
      {
        fractal_segment new_segment;

        new_segment.x1 = tmp_x1;
        new_segment.y1 = tmp_y1;
        new_segment.x2 = x;
        new_segment.y2 = y;
        new_segment.iterate = true;

        fractal_segments.push_back(new_segment);

        draw_state = 0;
      }

    gtk_widget_queue_draw(draw_area2);
    return FALSE;
  }

int main(int argc, char *argv[])
  {
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window),800,600);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    paned2 = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);

    button_render = gtk_button_new_with_label("render");
    button_clear = gtk_button_new_with_label("clear");

    gtk_box_pack_start(GTK_BOX(box),button_render,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),button_clear,false,false,2);

    draw_area = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(draw_area),"draw",G_CALLBACK(draw_callback),NULL);

    draw_area2 = gtk_drawing_area_new();
    gtk_widget_set_events(draw_area2,GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(draw_area2),"draw",G_CALLBACK(draw_callback2),NULL);

    g_signal_connect(G_OBJECT(draw_area2),"motion_notify_event",G_CALLBACK(mouse_motion_callback2),NULL);
    g_signal_connect(G_OBJECT(draw_area2),"button_press_event",G_CALLBACK(mouse_press_callback2),NULL);

    frame_menu = gtk_frame_new("menu");
    frame_render = gtk_frame_new("render");
    frame_draw = gtk_frame_new("draw");

    gtk_widget_set_margin_end(frame_menu,10);

    gtk_frame_set_shadow_type(GTK_FRAME(frame_menu),GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_render),GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_draw),GTK_SHADOW_NONE);

    gtk_container_add(GTK_CONTAINER(frame_render),draw_area);
    gtk_container_add(GTK_CONTAINER(frame_draw),draw_area2);
    gtk_container_add(GTK_CONTAINER(frame_menu),box);

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
