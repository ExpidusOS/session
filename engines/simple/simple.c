/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *                                                                              
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef XFCE_DISABLE_DEPRECATED
#undef XFCE_DISABLE_DEPRECATED
#endif

#include <X11/Xlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gmodule.h>

#include <libxfcegui4/libxfcegui4.h>

#include <libxfsm/xfsm-splash-engine.h>

#include <engines/simple/fallback.h>
#include <engines/simple/preview.h>


#define BORDER          2

#define DEFAULT_FONT    "Sans Bold 10"
#define DEFAULT_BGCOLOR "Black"
#define DEFAULT_FGCOLOR "White"


typedef struct _Simple Simple;
struct _Simple
{
  gboolean     dialog_active;
  GdkWindow   *window;
  GdkPixmap   *pixmap;
  GdkGC       *gc;
  PangoLayout *layout;
  GdkRectangle area;
  GdkRectangle textbox;
  GdkColor     bgcolor;
  GdkColor     fgcolor;
};


static GdkFilterReturn
simple_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  Simple *simple = (Simple *) user_data;
  XVisibilityEvent *xvisev = (XVisibilityEvent *) xevent;

  switch (xvisev->type)
    {
    case VisibilityNotify:
      if (!simple->dialog_active)
        {
          gdk_window_raise (simple->window);
          return GDK_FILTER_REMOVE;
        }
      break;
    }

  return GDK_FILTER_CONTINUE;
}


static void
simple_setup (XfsmSplashEngine *engine,
              XfsmSplashRc     *rc)
{
  PangoFontDescription *description;
  PangoFontMetrics     *metrics;
  PangoContext         *context;
  GdkWindowAttr         attr;
  GdkRectangle          geo;
  const gchar          *font;
  const gchar          *path;
  GdkWindow            *root;
  GdkPixbuf            *logo = NULL;
  GdkCursor            *cursor;
  Simple               *simple;
  gint                  logo_width;
  gint                  logo_height;
  gint                  text_height;

  simple = (Simple *) engine->user_data;

  /* load settings */
  gdk_color_parse (xfsm_splash_rc_read_entry (rc, "BgColor", DEFAULT_BGCOLOR),
                   &simple->bgcolor);
  gdk_color_parse (xfsm_splash_rc_read_entry (rc, "FgColor", DEFAULT_FGCOLOR),
                   &simple->fgcolor);
  font = xfsm_splash_rc_read_entry (rc, "Font", DEFAULT_FONT);
  path = xfsm_splash_rc_read_entry (rc, "Image", NULL);

  root = gdk_screen_get_root_window (engine->primary_screen);
  gdk_screen_get_monitor_geometry (engine->primary_screen,
                                   engine->primary_monitor,
                                   &geo);

  if (path != NULL && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    logo = gdk_pixbuf_new_from_file (path, NULL);
  if (logo == NULL)
    logo = gdk_pixbuf_from_pixdata (&fallback, FALSE, NULL);
  logo_width = gdk_pixbuf_get_width (logo);
  logo_height = gdk_pixbuf_get_height (logo);

  cursor = gdk_cursor_new (GDK_WATCH);

  /* create pango layout */
  description = pango_font_description_from_string (font);
  context = gdk_pango_context_get_for_screen (engine->primary_screen);
  pango_context_set_font_description (context, description);
  metrics = pango_context_get_metrics (context, description, NULL);
  text_height = (pango_font_metrics_get_ascent (metrics)
                 + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE
                + 4;

  simple->area.width = logo_width + 2 * BORDER;
  simple->area.height = logo_height + text_height + 3 * BORDER;
  simple->area.x = (geo.width - simple->area.width) / 2;
  simple->area.y = (geo.height - simple->area.height) / 2;

  simple->layout = pango_layout_new (context);
  simple->textbox.x = BORDER;
  simple->textbox.y = simple->area.height - (text_height + BORDER);
  simple->textbox.width = simple->area.width - 2 * BORDER;
  simple->textbox.height = text_height;

  /* create splash window */
  attr.x                  = simple->area.x;
  attr.y                  = simple->area.y;
  attr.event_mask         = GDK_VISIBILITY_NOTIFY_MASK;
  attr.width              = simple->area.width;
  attr.height             = simple->area.height;
  attr.wclass             = GDK_INPUT_OUTPUT;
  attr.window_type        = GDK_WINDOW_TEMP;
  attr.cursor             = cursor;
  attr.override_redirect  = TRUE;

  simple->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                  | GDK_WA_NOREDIR | GDK_WA_CURSOR);

  simple->pixmap = gdk_pixmap_new (simple->window,
                                   simple->area.width,
                                   simple->area.height,
                                   -1);

  gdk_window_set_back_pixmap (simple->window, simple->pixmap, FALSE);

  simple->gc = gdk_gc_new (simple->pixmap);
  gdk_gc_set_function (simple->gc, GDK_COPY);
  gdk_gc_set_rgb_fg_color (simple->gc, &simple->bgcolor);

  gdk_draw_rectangle (simple->pixmap,
                      simple->gc, TRUE,
                      0, 0,
                      simple->area.width,
                      simple->area.height);

  gdk_draw_pixbuf (simple->pixmap,
                   simple->gc,
                   logo,
                   0, 0,
                   BORDER, BORDER,
                   logo_width,
                   logo_height,
                   GDK_RGB_DITHER_NONE, 0, 0);

  gdk_window_add_filter (simple->window, simple_filter, simple);
  gdk_window_show (simple->window);

  /* cleanup */
  pango_font_description_free (description);
  pango_font_metrics_unref (metrics);
  gdk_cursor_unref (cursor);
  g_object_unref (context);
  g_object_unref (logo);
}


static void
simple_next (XfsmSplashEngine *engine, const gchar *text)
{
  Simple *simple = (Simple *) engine->user_data;
  GdkColor shcolor;
  gint tw, th, tx, ty;

  pango_layout_set_text (simple->layout, text, -1);
  pango_layout_get_pixel_size (simple->layout, &tw, &th);
  tx = simple->textbox.x + (simple->textbox.width - tw) / 2;
  ty = simple->textbox.y + (simple->textbox.height - th) / 2;

  gdk_gc_set_rgb_fg_color (simple->gc, &simple->bgcolor);
  gdk_draw_rectangle (simple->pixmap,
                      simple->gc, TRUE,
                      simple->textbox.x,
                      simple->textbox.y,
                      simple->textbox.width,
                      simple->textbox.height);

  gdk_gc_set_clip_rectangle (simple->gc, &simple->textbox);

  /* draw shadow */
  shcolor.red = (simple->fgcolor.red + simple->bgcolor.red) / 2;
  shcolor.green = (simple->fgcolor.green + simple->bgcolor.green) / 2;
  shcolor.blue = (simple->fgcolor.blue + simple->bgcolor.blue) / 2;
  shcolor.red = (shcolor.red + shcolor.green + shcolor.blue) / 3;
  shcolor.green = shcolor.red;
  shcolor.blue = shcolor.red;

  gdk_gc_set_rgb_fg_color (simple->gc, &shcolor);
  gdk_draw_layout (simple->pixmap, simple->gc,
                   tx + 2, ty + 2, simple->layout);

  gdk_gc_set_rgb_fg_color (simple->gc, &simple->fgcolor);
  gdk_draw_layout (simple->pixmap,
                   simple->gc, 
                   tx, ty,
                   simple->layout);

  gdk_window_clear_area (simple->window,
                         simple->textbox.x,
                         simple->textbox.y,
                         simple->textbox.width,
                         simple->textbox.height);
}


static int
simple_run (XfsmSplashEngine *engine,
            GtkWidget        *dialog)
{
  Simple *simple = (Simple *) engine->user_data;
  GtkRequisition requisition;
  int result;
  int x;
  int y;

  simple->dialog_active = TRUE;

  gtk_widget_size_request (dialog, &requisition);
  x = simple->area.x + (simple->area.width - requisition.width) / 2;
  y = simple->area.y + (simple->area.height - requisition.height) / 2;
  gtk_window_move (GTK_WINDOW (dialog), x, y);
  result = gtk_dialog_run (GTK_DIALOG (dialog));

  simple->dialog_active = FALSE;

  return result;
}


static void
simple_destroy (XfsmSplashEngine *engine)
{
  Simple *simple = (Simple *) engine->user_data;

  gdk_window_remove_filter (simple->window, simple_filter, simple);
  gdk_window_destroy (simple->window);
  g_object_unref (simple->layout);
  g_object_unref (simple->pixmap);
  g_object_unref (simple->gc);
  g_free (engine->user_data);
}


G_MODULE_EXPORT void
engine_init (XfsmSplashEngine *engine)
{
  Simple *simple;

  simple = g_new0 (Simple, 1);

  engine->user_data = simple;
  engine->setup = simple_setup;
  engine->next = simple_next;
  engine->run = simple_run;
  engine->destroy = simple_destroy;
}



static void
config_toggled (GtkWidget *button, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (button)));
}


static void
config_browse (GtkWidget *button, GtkWidget *entry)
{
  const gchar *filename;
  gchar       *new_filename;
  GtkWidget   *chooser;
  GtkWidget   *toplevel;

  toplevel = gtk_widget_get_toplevel (button);
  chooser = gtk_file_chooser_dialog_new (_("Choose image..."),
                                         GTK_WINDOW (toplevel),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);

  filename = gtk_entry_get_text (GTK_ENTRY (entry));
  if (filename != NULL)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);

  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), FALSE);

  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK)
    {
      new_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      gtk_entry_set_text (GTK_ENTRY (entry), new_filename);
      g_free (new_filename);
    }

  gtk_widget_destroy (chooser);
}


static void
config_configure (XfsmSplashConfig *config,
                  GtkWidget        *parent)
{
  const gchar *font;
  const gchar *path;
  GtkWidget   *dialog;
  GtkWidget   *frame;
  GtkWidget   *btn_font;
  GtkWidget   *table;
  GtkWidget   *label;
  GtkWidget   *sel_bg;
  GtkWidget   *sel_fg;
  GtkWidget   *checkbox;
  GtkWidget   *entry;
  GtkWidget   *image;
  GtkWidget   *button;
  GdkColor     color;
  GtkBox      *dbox;
  gchar        buffer[32];

  dialog = gtk_dialog_new_with_buttons (_("Configure Simple..."),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE,
                                        GTK_RESPONSE_CLOSE,
                                        NULL);

  dbox = GTK_BOX (GTK_DIALOG (dialog)->vbox);

  frame = xfce_framebox_new (_("Font"), FALSE);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  font = xfsm_splash_rc_read_entry (config->rc, "Font", DEFAULT_FONT);
  btn_font = gtk_font_button_new_with_font (font);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), btn_font);
  gtk_widget_show (btn_font);

  frame = xfce_framebox_new (_("Colors"), FALSE);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 2, FALSE);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), table);
  gtk_widget_show (table);

  label = gtk_label_new (_("Background color:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  gdk_color_parse (xfsm_splash_rc_read_entry (config->rc, "BgColor",
                                              DEFAULT_BGCOLOR), &color);
  sel_bg = xfce_color_button_new_with_color (&color);
  gtk_table_attach (GTK_TABLE (table), sel_bg, 1, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (sel_bg);

  label = gtk_label_new (_("Text color:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  gdk_color_parse (xfsm_splash_rc_read_entry (config->rc, "FgColor",
                                              DEFAULT_FGCOLOR), &color);
  sel_fg = xfce_color_button_new_with_color (&color);
  gtk_table_attach (GTK_TABLE (table), sel_fg, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (sel_fg);

  frame = xfce_framebox_new (_("Image"), FALSE);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 2, FALSE);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), table);
  gtk_widget_show (table);

  checkbox = gtk_check_button_new_with_label (_("Use custom image"));
  gtk_table_attach (GTK_TABLE (table), checkbox, 0, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (checkbox);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), entry, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (entry);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (button);

  path = xfsm_splash_rc_read_entry (config->rc, "Image", NULL);
  if (path == NULL || !g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), FALSE);
      gtk_widget_set_sensitive (entry, FALSE);
      gtk_widget_set_sensitive (button, FALSE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
      gtk_widget_set_sensitive (entry, TRUE);
      gtk_widget_set_sensitive (button, TRUE);
      gtk_entry_set_text (GTK_ENTRY (entry), path);
    }
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (config_toggled), entry);
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (config_toggled), button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (config_browse), entry);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* store settings */
  font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (btn_font));
  xfsm_splash_rc_write_entry (config->rc, "Font", font);

  xfce_color_button_get_color (XFCE_COLOR_BUTTON (sel_bg), &color);
  g_snprintf (buffer, 32, "#%02x%02x%02x",
              (unsigned) color.red >> 8,
              (unsigned) color.green >> 8,
              (unsigned) color.blue >> 8);
  xfsm_splash_rc_write_entry (config->rc, "BgColor", buffer);

  xfce_color_button_get_color (XFCE_COLOR_BUTTON (sel_fg), &color);
  g_snprintf (buffer, 32, "#%02x%02x%02x",
              (unsigned) color.red >> 8,
              (unsigned) color.green >> 8,
              (unsigned) color.blue >> 8);
  xfsm_splash_rc_write_entry (config->rc, "FgColor", buffer);

  path = gtk_entry_get_text (GTK_ENTRY (entry));
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox))
      && path != NULL && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      xfsm_splash_rc_write_entry (config->rc, "Image", path);
    }
  else
    {
      xfsm_splash_rc_write_entry (config->rc, "Image", "");
    }
  
  gtk_widget_destroy (dialog);
}


static GdkPixbuf*
config_preview (XfsmSplashConfig *config)
{
  return gdk_pixbuf_from_pixdata (&preview, FALSE, NULL);
}


G_MODULE_EXPORT void
config_init (XfsmSplashConfig *config)
{
  config->name        = g_strdup (_("Simple"));
  config->description = g_strdup (_("Simple Splash Engine"));
  config->version     = g_strdup (VERSION);
  config->author      = g_strdup ("Benedikt Meurer");
  config->homepage    = g_strdup ("http://www.xfce.org/");
  
  config->configure   = config_configure;
  config->preview     = config_preview;
}


