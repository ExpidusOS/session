/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <math.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <engines/balou/balou-theme.h>


static void       load_color_pair (const XfceRc *rc,
                                   const gchar  *name,
                                   GdkRGBA      *color1_return,
                                   GdkRGBA      *color2_return,
                                   const gchar  *color_default);
static GdkPixbuf  *load_pixbuf    (const gchar *path,
                                   gint         available_width,
                                   gint         available_height);
static time_t mtime (const gchar *path);
static GdkPixbuf *load_cached_preview (const BalouTheme *theme);
static void store_cached_preview (const BalouTheme *theme,
                                  GdkPixbuf        *pixbuf);


struct _BalouTheme
{
  GdkRGBA   bgcolor1;
  GdkRGBA   bgcolor2;
  GdkRGBA   fgcolor;
  gchar    *name;
  gchar    *description;
  gchar    *font;
  gchar    *theme_file;
  gchar    *logo_file;
};


#define DEFAULT_BGCOLOR "White"
#define DEFAULT_FGCOLOR "Black"
#define DEFAULT_FONT    "Sans Bold 12"


BalouTheme*
balou_theme_load (const gchar *name)
{
  BalouTheme  *theme;
  const gchar *image_file;
  const gchar *spec;
  gchar       *resource;
  gchar       *file;
  XfceRc      *rc;

  theme = g_new0 (BalouTheme, 1);

  resource = g_strdup_printf ("%s/balou/themerc", name);
  file = xfce_resource_lookup (XFCE_RESOURCE_THEMES, resource);
  g_free (resource);

  if (file != NULL)
    {
      rc = xfce_rc_simple_open (file, TRUE);
      if (rc == NULL)
        {
          g_free (file);
          goto set_defaults;
        }

      theme->theme_file = g_strdup (file);

      xfce_rc_set_group (rc, "Info");
      theme->name = g_strdup (xfce_rc_read_entry (rc, "Name", name));
      theme->description = g_strdup (xfce_rc_read_entry (rc, "Description", _("No description given")));

      xfce_rc_set_group (rc, "Splash Screen");
      load_color_pair (rc, "bgcolor", &theme->bgcolor1, &theme->bgcolor2,
                       DEFAULT_BGCOLOR);

      spec = xfce_rc_read_entry (rc, "fgcolor", DEFAULT_FGCOLOR);
      if (!gdk_rgba_parse (&theme->fgcolor, spec))
        gdk_rgba_parse (&theme->fgcolor, DEFAULT_FGCOLOR);

      spec = xfce_rc_read_entry (rc, "font", DEFAULT_FONT);
      theme->font = g_strdup (spec);

      image_file = xfce_rc_read_entry (rc, "logo", NULL);
      if (image_file != NULL)
        {
          resource = g_path_get_dirname (file);
          theme->logo_file = g_build_filename (resource, image_file, NULL);
          g_free (resource);
        }
      else
        {
          theme->logo_file = NULL;
        }

      xfce_rc_close (rc);
      g_free (file);

      return theme;
    }

set_defaults:
  gdk_rgba_parse (&theme->bgcolor1, DEFAULT_BGCOLOR);
  gdk_rgba_parse (&theme->bgcolor2, DEFAULT_BGCOLOR);
  gdk_rgba_parse (&theme->fgcolor, DEFAULT_FGCOLOR);
  theme->font = g_strdup (DEFAULT_FONT);
  theme->logo_file = NULL;

  return theme;
}


const gchar*
balou_theme_get_name (const BalouTheme *theme)
{
  return theme->name;
}


const gchar*
balou_theme_get_description (const BalouTheme *theme)
{
  return theme->description;
}


const gchar*
balou_theme_get_font (const BalouTheme *theme)
{
  return theme->font;
}


void
balou_theme_get_bgcolor (const BalouTheme *theme,
                         GdkRGBA          *color_return)
{
  *color_return = theme->bgcolor1;
}


void
balou_theme_get_fgcolor (const BalouTheme *theme,
                         GdkRGBA          *color_return)
{
  *color_return = theme->fgcolor;
}


GdkPixbuf*
balou_theme_get_logo (const BalouTheme *theme,
                      gint              available_width,
                      gint              available_height)
{
  return load_pixbuf (theme->logo_file,
                      available_width,
                      available_height);
}



void
balou_theme_draw_gradient (const BalouTheme *theme,
                           cairo_t          *cr,
                           GdkRectangle      logobox,
                           GdkRectangle      textbox)
{
  GdkRGBA  color;
  gint     dred;
  gint     dgreen;
  gint     dblue;
  gint     i;

  if (gdk_rgba_equal (&theme->bgcolor1, &theme->bgcolor2))
    {
      gdk_cairo_set_source_rgba (cr, &theme->bgcolor1);

      gdk_cairo_rectangle (cr, &logobox);
      cairo_fill (cr);

      gdk_cairo_rectangle (cr, &textbox);
      cairo_fill (cr);
    }
  else
    {
      /* calculate differences */
      dred = theme->bgcolor1.red - theme->bgcolor2.red;
      dgreen = theme->bgcolor1.green - theme->bgcolor2.green;
      dblue = theme->bgcolor1.blue - theme->bgcolor2.blue;

      for (i = 0; i < logobox.height; ++i)
        {
          color.red = theme->bgcolor2.red + (i * dred / logobox.height);
          color.green = theme->bgcolor2.green + (i * dgreen / logobox.height);
          color.blue = theme->bgcolor2.blue + (i * dblue / logobox.height);

          gdk_cairo_set_source_rgba (cr, &color);
          cairo_move_to(cr, logobox.x, logobox.y + i);
          cairo_line_to(cr, logobox.x + logobox.width, logobox.y + i);
          cairo_stroke(cr);
        }

    if (textbox.width != 0 && textbox.height != 0)
      {
        gdk_cairo_set_source_rgba (cr, &theme->bgcolor1);
        gdk_cairo_rectangle (cr, &textbox);
        cairo_fill(cr);
      }
    }
}


GdkPixbuf*
balou_theme_generate_preview (const BalouTheme *theme,
                              gint              width,
                              gint              height)
{
#define WIDTH   320
#define HEIGHT  240

  GdkRectangle logobox;
  GdkRectangle textbox;
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled;
  GdkWindow *root;
  cairo_surface_t *surface;
  cairo_t   *cr;
  gint       pw, ph;

  /* check for a cached preview first */
  pixbuf = load_cached_preview (theme);
  if (pixbuf != NULL)
    {
      pw = gdk_pixbuf_get_width (pixbuf);
      ph = gdk_pixbuf_get_height (pixbuf);

      if (pw == width && ph == height)
        {
          return pixbuf;
        }
      else if (pw >= width && ph >= height)
        {
          scaled = gdk_pixbuf_scale_simple (pixbuf, width, height,
                                            GDK_INTERP_BILINEAR);
          g_object_unref (pixbuf);
          return scaled;
        }

      g_object_unref (pixbuf);
    }

  root = gdk_screen_get_root_window (gdk_screen_get_default ());
  surface = gdk_window_create_similar_surface (root,
                                               CAIRO_CONTENT_COLOR_ALPHA,
                                               gdk_window_get_width (root),
                                               gdk_window_get_height (root));
  cr = cairo_create(surface);

  logobox.x = 0;
  logobox.y = 0;
  logobox.width = WIDTH;
  logobox.height = HEIGHT;
  textbox.x = 0;
  textbox.y = 0;
  balou_theme_draw_gradient (theme, cr, logobox, textbox);

  pixbuf = balou_theme_get_logo (theme, WIDTH, HEIGHT);
  if (pixbuf != NULL)
    {
      pw = gdk_pixbuf_get_width (pixbuf);
      ph = gdk_pixbuf_get_height (pixbuf);

      gdk_cairo_set_source_pixbuf (cr, pixbuf, (WIDTH - pw) / 2, (HEIGHT - ph) / 2);
      cairo_paint (cr);

      g_object_unref (G_OBJECT (pixbuf));
    }

  cairo_surface_flush (surface);

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, WIDTH, HEIGHT);
  scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);

  g_object_unref (pixbuf);
  cairo_destroy(cr);
  cairo_surface_destroy (surface);

  /* store preview */
  store_cached_preview (theme, scaled);

  return scaled;

#undef WIDTH
#undef HEIGHT
}


void
balou_theme_destroy (BalouTheme *theme)
{
  if (theme->name != NULL)
    g_free (theme->name);
  if (theme->description != NULL)
    g_free (theme->description);
  if (theme->theme_file != NULL)
    g_free (theme->theme_file);
  if (theme->logo_file != NULL)
    g_free (theme->logo_file);
  g_free (theme);
}



static void
load_color_pair (const XfceRc *rc,
                 const gchar  *name,
                 GdkRGBA      *color1_return,
                 GdkRGBA      *color2_return,
                 const gchar  *color_default)
{
  const gchar *spec;
  gchar      **s;

  spec = xfce_rc_read_entry (rc, name, color_default);
  if (spec == NULL)
    {
      gdk_rgba_parse (color1_return, color_default);
      gdk_rgba_parse (color2_return, color_default);
    }
  else
    {
      s = g_strsplit (spec, ":", 2);

      if (s[0] == NULL)
        {
          gdk_rgba_parse (color1_return, color_default);
          gdk_rgba_parse (color2_return, color_default);
        }
      else if (s[1] == NULL)
        {
          if (!gdk_rgba_parse (color1_return, s[0]))
            gdk_rgba_parse (color1_return, color_default);
          *color2_return = *color1_return;
        }
      else
        {
          if (!gdk_rgba_parse (color2_return, s[0]))
            gdk_rgba_parse (color2_return, color_default);
          if (!gdk_rgba_parse (color1_return, s[1]))
            *color1_return = *color2_return;
        }

      g_strfreev (s);
    }
}


static GdkPixbuf*
load_pixbuf (const gchar *path,
             gint         available_width,
             gint         available_height)
{
  static char *suffixes[] = { "svg", "png", "jpeg", "jpg", "xpm", NULL };
  GdkPixbuf *scaled;
  GdkPixbuf *pb = NULL;
  gint pb_width;
  gint pb_height;
  gdouble wratio;
  gdouble hratio;
  gchar *file;
  guint n;

  if (G_UNLIKELY (path == NULL))
    return NULL;

  pb = gdk_pixbuf_new_from_file (path, NULL);
  if (G_UNLIKELY (pb == NULL))
    {
      for (n = 0; pb == NULL && suffixes[n] != NULL; ++n)
        {
          file = g_strdup_printf ("%s.%s", path, suffixes[n]);
          pb = gdk_pixbuf_new_from_file (file, NULL);
          g_free (file);
        }
    }

  if (G_UNLIKELY (pb == NULL))
    return NULL;

  pb_width = gdk_pixbuf_get_width (pb);
  pb_height = gdk_pixbuf_get_height (pb);

  if (pb_width > available_width || pb_height > available_height)
    {
      wratio = (gdouble) pb_width / (gdouble) available_width;
      hratio = (gdouble) pb_height / (gdouble) available_height;

      if (hratio > wratio)
        {
          pb_width = rint (pb_width / hratio);
          pb_height = available_height;
        }
      else
        {
          pb_width = available_width;
          pb_height = rint (pb_height / wratio);
        }

      scaled = gdk_pixbuf_scale_simple (pb,
                                        pb_width,
                                        pb_height,
                                        GDK_INTERP_BILINEAR);
      g_object_unref (pb);
      pb = scaled;
    }

  return pb;
}


static time_t
mtime (const gchar *path)
{
  struct stat sb;

  if (path == NULL || stat (path, &sb) < 0)
    return (time_t) 0;

  return sb.st_mtime;
}


static GdkPixbuf*
load_cached_preview (const BalouTheme *theme)
{
  GdkPixbuf *pixbuf;
  gchar     *resource;
  gchar     *preview;

  resource = g_strconcat ("splash-theme-preview-", theme->name, ".png", NULL);
  preview = xfce_resource_lookup (XFCE_RESOURCE_CACHE, resource);
  g_free (resource);

  if (preview == NULL)
    return NULL;

  if ((mtime (preview) < mtime (theme->theme_file))
      || (theme->logo_file != NULL
        && (mtime (preview) < mtime (theme->logo_file))))
    {
      /* preview is outdated, need to regenerate preview */
      unlink (preview);
      g_free (preview);

      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (preview, NULL);
  g_free (preview);

  return pixbuf;
}


static void
store_cached_preview (const BalouTheme *theme,
                      GdkPixbuf        *pixbuf)
{
  gchar *resource;
  gchar *preview;

  resource = g_strconcat ("splash-theme-preview-", theme->name, ".png", NULL);
  preview = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource, TRUE);
  g_free (resource);

  if (preview != NULL)
    {
      gdk_pixbuf_save (pixbuf, preview, "png", NULL, NULL);
      g_free (preview);
    }
}



