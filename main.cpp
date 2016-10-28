#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

GtkWidget *window;
GtkWidget *paned;
GtkWidget *paned2;
GtkWidget *box;

GtkWidget *button_render;
GtkWidget *button_clear;

GtkWidget *separator;

GtkWidget *iterate_toggle;

GtkWidget *iteration_input;
GtkWidget *line_width_input;

GtkWidget *frame_menu;
GtkWidget *frame_render;
GtkWidget *frame_draw;

GtkWidget *draw_area;
GtkWidget *draw_area2;

typedef struct
  {
    double x1;      // pixel coordinates
    double y1;
    double x2;
    double y2;
    bool iterate;   // if true, the segment will be iterated on
  } fractal_segment;

typedef struct         // represents a transform in the following order: rotation (CW, around center), scale (around center), translate (from original center to original center 2)
  {
    double rotation;   // in radians
    double scale;
    double translate_x;
    double translate_y;
  } transform;

vector<fractal_segment> fractal_segments;  // segments the user has drawn
vector<fractal_segment> iterated_fractal_segments;
int draw_state = 0;                        // 0 - waiting for point 1, 1 - waiting for point 2

double tmp_x1;
double tmp_y1;
double cursor_x;
double cursor_y;

void print_segment(fractal_segment s)
  {
    cout << "[" << s.x1 << "," << s.y1 << "] [" << s.x2 << "," << s.y2 << "]" << endl;
  }

void print_segments(vector<fractal_segment> v)
  {
    for (int i = 0; i < v.size(); i++)
      print_segment(v[i]);
  }

void print_transform(transform t)
  {
    cout << "transform:" << endl;
    cout << "  rotation: " << t.rotation << " (" << (t.rotation / (2 * G_PI)) * 360 << " deg)" << endl;
    cout << "  scale: " << t.scale << endl;
    cout << "  translation: [" << t.translate_x << "," << t.translate_y << "]" << endl;
  }

transform segment_to_segment_transform(fractal_segment segment_from, fractal_segment segment_to)
  {
    transform result;
    
    double center1_x = (segment_from.x1 + segment_from.x2) / 2.0;
    double center1_y = (segment_from.y1 + segment_from.y2) / 2.0;
    double center2_x = (segment_to.x1 + segment_to.x2) / 2.0;
    double center2_y = (segment_to.y1 + segment_to.y2) / 2.0;

    result.translate_x = center2_x - center1_x;
    result.translate_y = center2_y - center1_y;

    double vec1_x = segment_from.x2 - segment_from.x1;
    double vec1_y = segment_from.y2 - segment_from.y1;
    double vec1_length = sqrt(vec1_x * vec1_x + vec1_y * vec1_y);

    vec1_x /= vec1_length;   // normalize
    vec1_y /= vec1_length;

    double vec2_x = segment_to.x2 - segment_to.x1;
    double vec2_y = segment_to.y2 - segment_to.y1;
    double vec2_length = sqrt(vec2_x * vec2_x + vec2_y * vec2_y);

    vec2_x /= vec2_length;   // normalize
    vec2_y /= vec2_length;

    result.rotation = acos(vec1_x * vec2_x + vec1_y * vec2_y);       // angle between vec1 and vec2, dot product

    double z = vec1_x * vec2_y - vec1_y * vec2_x;                    // z component of cross product, to determine CW/CCW

    if (z < 0)
      result.rotation *= -1;
      
    result.scale = vec2_length / vec1_length;

    return result;
  }

fractal_segment apply_transform_to_segment(fractal_segment segment, transform what_transform, double center_x, double center_y)
  {
    // move to center
    segment.x1 -= center_x;   
    segment.y1 -= center_y;
    segment.x2 -= center_x;
    segment.y2 -= center_y;
        
    // scale
    segment.x1 *= what_transform.scale;
    segment.y1 *= what_transform.scale;
    segment.x2 *= what_transform.scale;
    segment.y2 *= what_transform.scale;

    // rotate
    double s = sin(what_transform.rotation);
    double c = cos(what_transform.rotation);

    double x = segment.x1;
    double y = segment.y1;

    segment.x1 = x * c - y * s;
    segment.y1 = x * s + y * c;
 
    x = segment.x2;
    y = segment.y2;

    segment.x2 = x * c - y * s;
    segment.y2 = x * s + y * c;

    // move to original position
    segment.x1 += center_x;   
    segment.y1 += center_y;
    segment.x2 += center_x;
    segment.y2 += center_y;

    // translate
    segment.x1 += what_transform.translate_x;
    segment.y1 += what_transform.translate_y;
    segment.x2 += what_transform.translate_x;
    segment.y2 += what_transform.translate_y;

    return segment;
  }

void line_width_change_callback()
  {
    gtk_widget_queue_draw(draw_area);
  }

gboolean draw_callback(GtkWidget *widget,cairo_t *cr,gpointer data)
  {
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_set_line_width(cr,gtk_spin_button_get_value(GTK_SPIN_BUTTON(line_width_input)));

    cairo_set_source_rgb(cr,0,0,0);        

    for (int i = 0; i < iterated_fractal_segments.size(); i++)
      {
        fractal_segment segment = iterated_fractal_segments[i];
        cairo_move_to(cr,segment.x1,segment.y1);
        cairo_line_to(cr,segment.x2,segment.y2);
        cairo_stroke(cr);
      }

    return FALSE;
  }

void generate_fractal(int iterations)
  {
    iterated_fractal_segments.clear();

    if (fractal_segments.size() == 0)
      return;

    for (int i = 0; i < fractal_segments.size(); i++) // copy the pattern
      iterated_fractal_segments.push_back(fractal_segments[i]);

    vector<fractal_segment> new_segments;

    double line_center_x = (fractal_segments[0].x1 + fractal_segments[fractal_segments.size() - 1].x2) / 2.0; 
    double line_center_y = (fractal_segments[0].y1 + fractal_segments[fractal_segments.size() - 1].y2) / 2.0;

    for (int iteration = 0; iteration < iterations; iteration++)
      {
        cout << "iteration " << iteration + 1 << endl;

        new_segments.clear();

        for (int i = 0; i < iterated_fractal_segments.size(); i++)
          {
            transform t;
            fractal_segment s;

            s.x1 = fractal_segments[0].x1;
            s.y1 = fractal_segments[0].y1;
            s.x2 = fractal_segments[fractal_segments.size() - 1].x2;
            s.y2 = fractal_segments[fractal_segments.size() - 1].y2;

            t = segment_to_segment_transform(s,iterated_fractal_segments[i]);

            if (iterated_fractal_segments[i].iterate)
              {
                for (int j = 0; j < fractal_segments.size(); j++)
                  {
                    fractal_segment transformed_segment = fractal_segments[j];
                    transformed_segment = apply_transform_to_segment(transformed_segment,t,line_center_x,line_center_y);       
                    new_segments.push_back(transformed_segment);
                  }
              }
            else
              {
                new_segments.push_back(iterated_fractal_segments[i]);
              }
          }

        iterated_fractal_segments.clear();

        for (int i = 0; i < new_segments.size(); i++)
          iterated_fractal_segments.push_back(new_segments[i]);
      }
  }

gboolean clear_clicked_callback(GtkButton *button,gpointer user_data)
  {
    fractal_segments.clear();
    gtk_widget_queue_draw(draw_area2);
  }

gboolean render_clicked_callback(GtkButton *button,gpointer user_data)
  {
    cout << "rendering" << endl;
    generate_fractal(gtk_spin_button_get_value(GTK_SPIN_BUTTON(iteration_input)));
    cout << "segments: " << iterated_fractal_segments.size() << endl;
    gtk_widget_queue_draw(draw_area);
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

    cairo_set_line_width(cr,2);

    for (int i = 0; i < fractal_segments.size(); i++)
      {
        fractal_segment segment = fractal_segments[i];

        if (segment.iterate)
          cairo_set_source_rgb(cr,0,0,0);
        else
          cairo_set_source_rgb(cr,0.5,0.5,1);        

        cairo_move_to(cr,segment.x1,segment.y1);
        cairo_line_to(cr,segment.x2,segment.y2);
        cairo_stroke(cr);
      }

    if (draw_state == 1)
      {
        cairo_move_to(cr,tmp_x1,tmp_y1);
        cairo_line_to(cr,cursor_x,cursor_y);
        cairo_stroke(cr);
      }

    if (fractal_segments.size() > 0)
      {
        static const double dashed3[] = {10.0};
        cairo_set_dash(cr,dashed3,1,0);

        cairo_set_source_rgb(cr,0.5,0.5,0.7);
        cairo_move_to(cr,fractal_segments[0].x1,fractal_segments[0].y1);
        cairo_line_to(cr,fractal_segments[fractal_segments.size() - 1].x2,fractal_segments[fractal_segments.size() - 1].y2);
        cairo_stroke(cr);
       
        cairo_set_dash(cr,NULL,0,0);

        cairo_set_source_rgb(cr,1,0,0);
        cairo_arc(cr,fractal_segments[0].x1,fractal_segments[0].y1,5,0,2 * G_PI);
        cairo_fill(cr);

        cairo_set_source_rgb(cr,0,1,0);
        cairo_arc(cr,fractal_segments[fractal_segments.size() - 1].x2,fractal_segments[fractal_segments.size() - 1].y2,5,0,2 * G_PI);
        cairo_fill(cr);
      }

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

    x = event->x; //   / double(width);
    y = event->y; //   / double(height);

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
        new_segment.iterate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(iterate_toggle));

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

    iteration_input = gtk_spin_button_new_with_range(0,100,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(iteration_input),4);

    line_width_input = gtk_spin_button_new_with_range(1,10,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(line_width_input),2);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    iterate_toggle = gtk_toggle_button_new_with_label("iterate segment");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iterate_toggle),true);

    gtk_box_pack_start(GTK_BOX(box),button_render,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("iterations"),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),iteration_input,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("line width"),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),line_width_input,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),separator,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),button_clear,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),iterate_toggle,false,false,2);

    draw_area = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(draw_area),"draw",G_CALLBACK(draw_callback),NULL);

    draw_area2 = gtk_drawing_area_new();
    gtk_widget_set_events(draw_area2,GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(draw_area2),"draw",G_CALLBACK(draw_callback2),NULL);

    g_signal_connect(G_OBJECT(draw_area2),"motion_notify_event",G_CALLBACK(mouse_motion_callback2),NULL);
    g_signal_connect(G_OBJECT(draw_area2),"button_press_event",G_CALLBACK(mouse_press_callback2),NULL);

    g_signal_connect(G_OBJECT(button_clear),"clicked",G_CALLBACK(clear_clicked_callback),NULL);
    g_signal_connect(G_OBJECT(button_render),"clicked",G_CALLBACK(render_clicked_callback),NULL);

    g_signal_connect(G_OBJECT(line_width_input),"value-changed",G_CALLBACK(line_width_change_callback),NULL);

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
    gtk_paned_set_position(GTK_PANED(paned2),300);

    gtk_container_add(GTK_CONTAINER(window),paned);

    gtk_window_set_title(GTK_WINDOW(window),"fractal generator");
    gtk_container_set_border_width(GTK_CONTAINER(window),10);
  
    gtk_widget_show_all(window);
  
    g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),NULL);  

    gtk_main();

    return 0;
  }
