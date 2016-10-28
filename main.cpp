// fractal generator, Miloslav Ciz 2016

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <iostream>
#include <vector>
#include <math.h>
#include <cstdlib>
#include <ctime>

#define PROGRAM_NAME "fractal generator"
#define PROGRAM_VERSION "1.0"

using namespace std;

GtkWidget *window;                      // main window
GtkWidget *paned;                       // main paned layout
GtkWidget *paned2;                      // secondary paned layout
GtkWidget *box;                         // menu box

GtkWidget *button_render;
GtkWidget *button_clear;
GtkWidget *button_about;
GtkWidget *button_random_example;

GtkWidget *iterate_toggle;
GtkWidget *show_iterations_toggle;

GtkWidget *iteration_input;             // numeric inputs
GtkWidget *line_width_input;

GtkWidget *frame_menu;                  // menu frame
GtkWidget *frame_render;                // frame in which the fractal will be shown
GtkWidget *frame_draw;                  // frame for pattern drawing

GtkWidget *draw_area;                   // for drawing the fractal
GtkWidget *draw_area2;                  // for drawing the pattern

GtkWidget *about_dialog;

typedef struct                          // a line segment of fractal
  {
    double x1;                          // pixel coordinates
    double y1;
    double x2;
    double y2;
    bool iterate;                       // if true, the segment will be iterated on during generation
    int iteration_number;
  } fractal_segment;

typedef struct                          // transform: rotation (CW, around center), scale (around center), translate
  {
    double rotation;                    // in radians
    double scale;
    double translate_x;
    double translate_y;
  } transform;

vector<fractal_segment> pattern_segments;          // pattern segments the user has drawn
vector<fractal_segment> iterated_fractal_segments;
int draw_state = 0;                                // 0 - waiting for point 1, 1 - waiting for point 2

double tmp_x1;          // holds coordinates of the first point when segment is being drawn
double tmp_y1;
double cursor_x;        // current cursor coordinates during segment drawing
double cursor_y;

void print_segment(fractal_segment s)              // for debugging
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

// Finds a transformation of one segment to another.
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

// Applies a transformation to given segment.
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

void redraw_callback()
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

    gtk_render_background(context,cr,0,0,width,height);

    cairo_set_line_width(cr,gtk_spin_button_get_value(GTK_SPIN_BUTTON(line_width_input)));

    for (int i = 0; i < iterated_fractal_segments.size(); i++)
      {
        fractal_segment segment = iterated_fractal_segments[i];

        double intensity = 0;

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_iterations_toggle)))
          intensity = segment.iteration_number / double(gtk_spin_button_get_value(GTK_SPIN_BUTTON(iteration_input)));
        
        cairo_set_source_rgb(cr,intensity,0,0);
        
        cairo_move_to(cr,segment.x1,segment.y1);
        cairo_line_to(cr,segment.x2,segment.y2);
        cairo_stroke(cr);
      }

    return FALSE;
  }

// Generates the fractal from pattern in pattern_segments to iterated_fractal_segments.
void generate_fractal(int iterations)
  {
    iterated_fractal_segments.clear();

    if (pattern_segments.size() == 0)
      return;

    for (int i = 0; i < pattern_segments.size(); i++) // copy the pattern
      {
        iterated_fractal_segments.push_back(pattern_segments[i]);
        iterated_fractal_segments[iterated_fractal_segments.size() - 1].iteration_number = 0;
      }

    vector<fractal_segment> new_segments;

    double line_center_x = (pattern_segments[0].x1 + pattern_segments[pattern_segments.size() - 1].x2) / 2.0; 
    double line_center_y = (pattern_segments[0].y1 + pattern_segments[pattern_segments.size() - 1].y2) / 2.0;

    for (int iteration = 0; iteration < iterations; iteration++)
      {
        cout << "iteration " << iteration + 1 << endl;

        new_segments.clear();

        for (int i = 0; i < iterated_fractal_segments.size(); i++)
          {
            transform t;
            fractal_segment s;

            s.x1 = pattern_segments[0].x1;
            s.y1 = pattern_segments[0].y1;
            s.x2 = pattern_segments[pattern_segments.size() - 1].x2;
            s.y2 = pattern_segments[pattern_segments.size() - 1].y2;

            int segment_iteration_number = iterated_fractal_segments[i].iteration_number;

            t = segment_to_segment_transform(s,iterated_fractal_segments[i]);

            if (iterated_fractal_segments[i].iterate)
              {
                for (int j = 0; j < pattern_segments.size(); j++)
                  {
                    fractal_segment transformed_segment = pattern_segments[j];
                    transformed_segment = apply_transform_to_segment(transformed_segment,t,line_center_x,line_center_y);  
                    transformed_segment.iteration_number = segment_iteration_number + 1;     
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
    pattern_segments.clear();
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

    for (int i = 0; i < pattern_segments.size(); i++)
      {
        fractal_segment segment = pattern_segments[i];

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

    if (pattern_segments.size() > 0)
      {
        static const double dashed3[] = {10.0};
        cairo_set_dash(cr,dashed3,1,0);

        cairo_set_source_rgb(cr,0.5,0.5,0.7);
        cairo_move_to(cr,pattern_segments[0].x1,pattern_segments[0].y1);
        cairo_line_to(cr,pattern_segments[pattern_segments.size() - 1].x2,pattern_segments[pattern_segments.size() - 1].y2);
        cairo_stroke(cr);
       
        cairo_set_dash(cr,NULL,0,0);

        cairo_set_source_rgb(cr,1,0,0);
        cairo_arc(cr,pattern_segments[0].x1,pattern_segments[0].y1,5,0,2 * G_PI);
        cairo_fill(cr);

        cairo_set_source_rgb(cr,0,1,0);
        cairo_arc(cr,pattern_segments[pattern_segments.size() - 1].x2,pattern_segments[pattern_segments.size() - 1].y2,5,0,2 * G_PI);
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

        pattern_segments.push_back(new_segment);

        draw_state = 0;
      }

    cursor_x = event->x;
    cursor_y = event->y;

    gtk_widget_queue_draw(draw_area2);
    return FALSE;
  }

void about_clicked_callback()
  {
    char *authors[] = {(char *) "Miloslav Ciz",NULL};

    gtk_show_about_dialog(GTK_WINDOW(about_dialog),
      "program name",PROGRAM_NAME,
      "version",PROGRAM_VERSION,
      "authors",authors,
      "comments","Simple fractal generator program,\nmade as a school project for FIT BUT.",
      NULL);
  }

void append_pattern_segment(double x1, double y1, double x2, double y2, bool iterate)
  {
    fractal_segment s;
    s.x1 = x1;
    s.y1 = y1;
    s.x2 = x2;
    s.y2 = y2;
    s.iterate = iterate;
    pattern_segments.push_back(s);
  }

void random_example_clicked_callback()
  {
    int random;

    pattern_segments.clear();

    srand(std::time(0));

    random = rand() % 8;

    switch (random)
      {
        case 0:        // Koch curve
          append_pattern_segment(100, 200,    200, 200,     true);
          append_pattern_segment(200, 200,    250, 120,     true);
          append_pattern_segment(250, 120,    300, 200,     true);
          append_pattern_segment(300, 200,    400, 200,     true);
          break;

        case 1:        // M
          append_pattern_segment(200, 200,    250, 100,     true);
          append_pattern_segment(250, 100,    300, 210,     false);
          append_pattern_segment(300, 210,    350, 100,     false);
          append_pattern_segment(350, 100,    400, 200,     true);
          break;

        case 2:        // branch
          append_pattern_segment(100, 120,    70,  200,     true);
          append_pattern_segment(70,  200,    200, 150,     true);
          append_pattern_segment(200, 150,    340, 150,     true);
          append_pattern_segment(340, 150,    510, 205,     true);
          break;

        case 4:        // spirals
          append_pattern_segment(100, 50,     100, 200,     false);
          append_pattern_segment(100, 200,    200, 200,     true);
          append_pattern_segment(200, 200,    250, 50,      true);
          append_pattern_segment(250, 50,     350, 50,      true);
          append_pattern_segment(350, 50,     350, 200,     false);
          break;

        case 5:        // hexagon
          append_pattern_segment(200, 150,    250, 70,      true);
          append_pattern_segment(250, 70,     350, 70,      true);
          append_pattern_segment(350, 70,     400, 150,     true);
          append_pattern_segment(200, 150,    250, 230,     true);
          append_pattern_segment(250, 230,    350, 230,     true);
          append_pattern_segment(350, 230,    400, 150,     true);
          break;

        case 6:        // tree
          append_pattern_segment(200, 230,    200, 140,     true);
          append_pattern_segment(200, 140,    170, 100,     true);
          append_pattern_segment(200, 140,    230, 100,     true);
          append_pattern_segment(200, 140,    200, 140,     false);
          break;

        case 7:        // fingers
          append_pattern_segment(100, 200,    100, 150,     false);
          append_pattern_segment(100, 150,    200, 150,     true);
          append_pattern_segment(200, 150,    200, 80,      false);
          append_pattern_segment(200, 80,     300, 80,      true);
          append_pattern_segment(300, 80,     300, 160,     false);
          append_pattern_segment(300, 160,    450, 160,     true);
          append_pattern_segment(450, 160,    450, 200,     false);
          break;

        default:
          break;
      }

    gtk_widget_queue_draw(draw_area2);
    render_clicked_callback(NULL,NULL);

    cout << random << endl;
  }

int main(int argc, char *argv[])
  {
    gtk_init(&argc, &argv);

    // create widgets:

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    paned2 = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);

    button_render = gtk_button_new_with_label("render");
    button_clear = gtk_button_new_with_label("clear");
    button_about = gtk_button_new_with_label("about");
    button_random_example = gtk_button_new_with_label("random example");

    iteration_input = gtk_spin_button_new_with_range(0,10,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(iteration_input),4);

    line_width_input = gtk_spin_button_new_with_range(1,10,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(line_width_input),2);

    iterate_toggle = gtk_toggle_button_new_with_label("iterate segment");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(iterate_toggle),true);

    show_iterations_toggle = gtk_toggle_button_new_with_label("show iterations");

    about_dialog = gtk_about_dialog_new();

    draw_area = gtk_drawing_area_new();
    
    draw_area2 = gtk_drawing_area_new();
    gtk_widget_set_events(draw_area2,GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK);

    frame_menu = gtk_frame_new("menu");
    frame_render = gtk_frame_new("fractal render");
    frame_draw = gtk_frame_new("pattern draw");

    // add widgets to containers:

    gtk_box_pack_start(GTK_BOX(box),button_render,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),show_iterations_toggle,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("iterations"),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),iteration_input,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("line width"),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),line_width_input,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("   "),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("   "),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("drawing"),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),button_clear,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),iterate_toggle,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("   "),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new("   "),false,false,2);
    gtk_box_pack_start(GTK_BOX(box),button_random_example,false,false,2);
    gtk_box_pack_start(GTK_BOX(box),button_about,false,false,2);

    gtk_paned_add1(GTK_PANED(paned),frame_menu);
    gtk_paned_add2(GTK_PANED(paned),paned2);

    gtk_paned_add1(GTK_PANED(paned2),frame_render);
    gtk_paned_add2(GTK_PANED(paned2),frame_draw);

    gtk_container_add(GTK_CONTAINER(window),paned);

    gtk_container_add(GTK_CONTAINER(frame_render),draw_area);
    gtk_container_add(GTK_CONTAINER(frame_draw),draw_area2);
    gtk_container_add(GTK_CONTAINER(frame_menu),box);

    // connect signals:

    g_signal_connect(G_OBJECT(draw_area),"draw",G_CALLBACK(draw_callback),NULL);
    g_signal_connect(G_OBJECT(draw_area2),"draw",G_CALLBACK(draw_callback2),NULL);

    g_signal_connect(G_OBJECT(draw_area2),"motion_notify_event",G_CALLBACK(mouse_motion_callback2),NULL);
    g_signal_connect(G_OBJECT(draw_area2),"button_press_event",G_CALLBACK(mouse_press_callback2),NULL);

    g_signal_connect(G_OBJECT(button_clear),"clicked",G_CALLBACK(clear_clicked_callback),NULL);
    g_signal_connect(G_OBJECT(button_render),"clicked",G_CALLBACK(render_clicked_callback),NULL);
    g_signal_connect(G_OBJECT(button_about),"clicked",G_CALLBACK(about_clicked_callback),NULL);
    g_signal_connect(G_OBJECT(button_random_example),"clicked",G_CALLBACK(random_example_clicked_callback),NULL);

    g_signal_connect(G_OBJECT(line_width_input),"value-changed",G_CALLBACK(redraw_callback),NULL);
    g_signal_connect(G_OBJECT(show_iterations_toggle),"toggled",G_CALLBACK(redraw_callback),NULL);

    g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),NULL);  

    // set additional properties:

    gtk_widget_set_margin_end(frame_menu,10);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_menu),GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_render),GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_draw),GTK_SHADOW_NONE);

    gtk_widget_set_hexpand(paned,true);
    gtk_widget_set_vexpand(paned,true);

    GdkRectangle preferred_menu_size;
    preferred_menu_size.x = 0;
    preferred_menu_size.y = 0;
    preferred_menu_size.width = 100;
    preferred_menu_size.x = 1000;

    gtk_widget_size_allocate(frame_menu,&preferred_menu_size);
    gtk_paned_set_position(GTK_PANED(paned),200);
    gtk_paned_set_position(GTK_PANED(paned2),300);

    gtk_window_set_title(GTK_WINDOW(window),PROGRAM_NAME);
    gtk_window_set_default_size(GTK_WINDOW(window),800,600);
    gtk_container_set_border_width(GTK_CONTAINER(window),10);
  
    gtk_widget_show_all(window);

    gtk_main();        // run

    return 0;
  }
