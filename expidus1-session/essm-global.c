/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPE_SH
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib/gprintf.h>
#include <gio/gio.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include <expidus1-session/essm-global.h>

#include <libessm/essm-util.h>


/* global variables */
gboolean          verbose = FALSE;


void
essm_enable_verbose (void)
{
  if (!verbose)
    {
      verbose = TRUE;
      printf ("expidus1-session: Session Manager running in verbose mode.\n");
    }
}

gboolean
essm_is_verbose_enabled (void)
{
  return verbose;
}

void
essm_verbose_real (const char *func,
                   const char *file,
                   int line,
                   const char *format,
                   ...)
{
  static FILE *fp = NULL;
  gchar       *logfile;
  va_list      valist;

  if (G_UNLIKELY (fp == NULL))
    {
      logfile = expidus_get_homefile (".expidus1-session.verbose-log", NULL);

      /* rename an existing log file to -log.last */
      if (logfile && g_file_test (logfile, G_FILE_TEST_EXISTS))
        {
          gchar *oldlogfile = g_strdup_printf ("%s.last", logfile);
          if (oldlogfile)
            {
              if (rename (logfile, oldlogfile) != 0)
                {
                  g_warning ("unable to rename logfile");
                }
              g_free (oldlogfile);
            }
        }

      if (logfile)
        {
          fp = fopen (logfile, "w");
          g_free (logfile);
          fprintf(fp, "log file opened\n");
        }
    }

  if (fp == NULL)
    {
      return;
    }

  fprintf (fp, "TRACE[%s:%d] %s(): ", file, line, func);
  va_start (valist, format);
  vfprintf (fp, format, valist);
  fflush (fp);
  va_end (valist);
}


gchar *
essm_generate_client_id (SmsConn sms_conn)
{
  static char *addr = NULL;
  static int   sequence = 0;
  char        *sms_id;
  char        *id = NULL;

  if (sms_conn != NULL)
    {
      sms_id = SmsGenerateClientID (sms_conn);
      if (sms_id != NULL)
        {
          id = g_strdup (sms_id);
          free (sms_id);
        }
    }

  if (id == NULL)
    {
      if (addr == NULL)
        {
          /*
           * Faking our IP address, the 0 below is "unknown"
           * address format (1 would be IP, 2 would be DEC-NET
           * format). Stolen from KDE :-)
           */
          addr = g_strdup_printf ("0%.8x", g_random_int ());
        }

      id = (char *) g_malloc (50);
      g_snprintf (id, 50, "1%s%.13ld%.10d%.4d", addr,
                  (long) time (NULL), (int) getpid (), sequence);
      sequence = (sequence + 1) % 10000;
    }

  return id;
}


GValue *
essm_g_value_new (GType gtype)
{
  GValue *value = g_new0 (GValue, 1);
  g_value_init (value, gtype);
  return value;
}


void
essm_g_value_free (GValue *value)
{
  if (G_LIKELY (value))
    {
      g_value_unset (value);
      g_free (value);
    }
}


static gboolean
essm_check_valid_exec (const gchar *exec)
{
  gboolean result = TRUE;
  gchar   *tmp;
  gchar   *p;

  if (*exec == '/')
    {
      result = (access (exec, X_OK) == 0);
    }
  else
    {
      tmp = g_strdup (exec);
      p = strchr (tmp, ' ');
      if (G_UNLIKELY (p != NULL))
        *p = '\0';

      p = g_find_program_in_path (tmp);
      g_free (tmp);

      if (G_UNLIKELY (p == NULL))
        {
          result = FALSE;
        }
      else
        {
          result = (access (p, X_OK) == 0);
          g_free (p);
        }
    }

  return result;
}

gint
essm_launch_desktop_files_on_shutdown (gboolean         start_at_spi,
                                       EssmShutdownType shutdown_type)
{
  switch (shutdown_type)
    {
    case ESSM_SHUTDOWN_LOGOUT:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_LOGOUT);

    case ESSM_SHUTDOWN_SHUTDOWN:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_SHUTDOWN);

    case ESSM_SHUTDOWN_RESTART:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_RESTART);

    case ESSM_SHUTDOWN_SUSPEND:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_SUSPEND);

    case ESSM_SHUTDOWN_HIBERNATE:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_HIBERNATE);

    case ESSM_SHUTDOWN_HYBRID_SLEEP:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_HYBRID_SLEEP);

    case ESSM_SHUTDOWN_SWITCH_USER:
      return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_SWITCH_USER);

    default:
      g_error ("Failed to convert shutdown type '%d' to run hook name.", shutdown_type);
      return FALSE;
    }
}



gint
essm_launch_desktop_files_on_login (gboolean start_at_spi)
{
  return essm_launch_desktop_files_on_run_hook (start_at_spi, ESSM_RUN_HOOK_LOGIN);
}



gint
essm_launch_desktop_files_on_run_hook (gboolean    start_at_spi,
                                       EssmRunHook run_hook)
{
  const gchar *try_exec;
  const gchar *type;
  EssmRunHook  run_hook_from_file;
  gchar       *exec;
  gboolean     startup_notify;
  gboolean     terminal;
  gboolean     skip;
  GError      *error = NULL;
  ExpidusRc      *rc;
  gchar      **files;
  gchar      **only_show_in;
  gchar      **not_show_in;
  gint         started = 0;
  gint         n, m;
  gchar       *filename;
  const gchar *pattern;
  gchar       *uri;

  /* pattern for only at-spi desktop files or everything */
  if (start_at_spi)
    pattern = "autostart/at-spi-*.desktop";
  else
    pattern = "autostart/*.desktop";

  files = expidus_resource_match (EXPIDUS_RESOURCE_CONFIG, pattern, TRUE);
  for (n = 0; files[n] != NULL; ++n)
    {
      rc = expidus_rc_config_open (EXPIDUS_RESOURCE_CONFIG, files[n], TRUE);
      if (G_UNLIKELY (rc == NULL))
        continue;

      expidus_rc_set_group (rc, "Desktop Entry");

      /* check the Hidden key */
      skip = expidus_rc_read_bool_entry (rc, "Hidden", FALSE);
      run_hook_from_file = expidus_rc_read_int_entry (rc, "RunHook", ESSM_RUN_HOOK_LOGIN);

      /* only execute scripts with match the requested run hook */
      if (run_hook != run_hook_from_file)
        skip = TRUE;

      if (G_LIKELY (!skip))
        {
          essm_verbose("hidden set\n");

          if (expidus_rc_read_bool_entry (rc, "X-EXPIDUS-Autostart-Override", FALSE))
            {
              /* override the OnlyShowIn check */
              skip = FALSE;
              essm_verbose ("X-EXPIDUS-Autostart-Override set, launching\n");
            }
          else
            {
              /* check the OnlyShowIn setting */
              only_show_in = expidus_rc_read_list_entry (rc, "OnlyShowIn", ";");
              if (G_UNLIKELY (only_show_in != NULL))
                {
                  /* check if "EXPIDUS" is specified */
                  for (m = 0, skip = TRUE; only_show_in[m] != NULL; ++m)
                    {
                      if (g_ascii_strcasecmp (only_show_in[m], "EXPIDUS") == 0)
                        {
                          skip = FALSE;
                          essm_verbose ("only show in EXPIDUS set, launching\n");
                          break;
                        }
                    }

                  g_strfreev (only_show_in);
                }
            }

          /* check the NotShowIn setting */
          not_show_in = expidus_rc_read_list_entry (rc, "NotShowIn", ";");
          if (G_UNLIKELY (not_show_in != NULL))
            {
              /* check if "Expidus" is not specified */
              for (m = 0; not_show_in[m] != NULL; ++m)
                if (g_ascii_strcasecmp (not_show_in[m], "EXPIDUS") == 0)
                  {
                    skip = TRUE;
                    essm_verbose ("NotShowIn Expidus set, skipping\n");
                    break;
                  }

              g_strfreev (not_show_in);
            }

          /* skip at-spi launchers if not in at-spi mode or don't skip
           * them no matter what the OnlyShowIn key says if only
           * launching at-spi */
          filename = g_path_get_basename (files[n]);
          if (g_str_has_prefix (filename, "at-spi-"))
            {
              skip = !start_at_spi;
              essm_verbose ("start_at_spi (a11y support), %s\n", skip ? "skipping" : "showing");
            }
          g_free (filename);
        }

      /* check the "Type" key */
      type = expidus_rc_read_entry (rc, "Type", NULL);
      if (G_UNLIKELY (!skip && type != NULL && g_ascii_strcasecmp (type, "Application") != 0))
        {
          skip = TRUE;
          essm_verbose ("Type == Application, skipping\n");
        }

      /* check the "TryExec" key */
      try_exec = expidus_rc_read_entry (rc, "TryExec", NULL);
      if (G_UNLIKELY (!skip && try_exec != NULL))
        {
          skip = !essm_check_valid_exec (try_exec);
          if (skip)
            essm_verbose ("TryExec set and essm_check_valid_exec failed, skipping\n");
        }

      /* expand the field codes */
      filename = expidus_resource_lookup (EXPIDUS_RESOURCE_CONFIG, files[n]);
      uri = g_filename_to_uri (filename, NULL, NULL);
      g_free (filename);
      exec = expidus_expand_desktop_entry_field_codes (expidus_rc_read_entry (rc, "Exec", NULL),
                                                    NULL,
                                                    expidus_rc_read_entry (rc, "Icon", NULL),
                                                    expidus_rc_read_entry (rc, "Name", NULL),
                                                    uri,
                                                    FALSE);
      g_free (uri);

      /* execute the item */
      if (G_LIKELY (!skip && exec != NULL))
        {
          /* query launch parameters */
          startup_notify = expidus_rc_read_bool_entry (rc, "StartupNotify", FALSE);
          terminal = expidus_rc_read_bool_entry (rc, "Terminal", FALSE);

          /* try to launch the command */
          essm_verbose ("Autostart: running command \"%s\"\n", exec);
          if (!expidus_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                  exec,
                                                  terminal,
                                                  startup_notify,
                                                  &error))
            {
              g_warning ("Unable to launch \"%s\" (specified by %s): %s", exec, files[n], error->message);
              essm_verbose ("Unable to launch \"%s\" (specified by %s): %s\n", exec, files[n], error->message);
              g_error_free (error);
              error = NULL;
            }
          else
            {
              ++started;
            }
        }

      /* cleanup */
      expidus_rc_close (rc);
      g_free (exec);
    }
  g_strfreev (files);

  return started;
}
