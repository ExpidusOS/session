/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@expidus.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gio/gio.h>

#include <esconf/esconf.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include <libessm/essm-util.h>

#include <expidus1-session/ice-layer.h>
#include <expidus1-session/sm-layer.h>
#include <expidus1-session/essm-dns.h>
#include <expidus1-session/essm-global.h>
#include <expidus1-session/essm-manager.h>
#include <expidus1-session/essm-shutdown.h>
#include <expidus1-session/essm-startup.h>
#include <expidus1-session/essm-error.h>

static gboolean opt_disable_tcp = FALSE;
static gboolean opt_version = FALSE;
static EsconfChannel *channel = NULL;
static guint name_id = 0;

static GOptionEntry option_entries[] =
{
  { "disable-tcp", '\0', 0, G_OPTION_ARG_NONE, &opt_disable_tcp, N_("Disable binding to TCP ports"), NULL },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Print version information and exit"), NULL },
  { NULL }
};


static void name_lost (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data);


static void
setup_environment (void)
{
  const gchar *sm;
  gchar       *authfile;
  int          fd;

  /* check that no other session manager is running */
  sm = g_getenv ("SESSION_MANAGER");
  if (sm != NULL && strlen (sm) > 0)
    {
      g_printerr ("%s: Another session manager is already running\n", PACKAGE_NAME);
      exit (EXIT_FAILURE);
    }

  /* check if running in verbose mode */
  if (g_getenv ("ESSM_VERBOSE") != NULL)
    essm_enable_verbose ();

  /* pass correct DISPLAY to children, in case of --display in argv */
  g_setenv ("DISPLAY", gdk_display_get_name (gdk_display_get_default ()), TRUE);

  /* check access to $ICEAUTHORITY or $HOME/.ICEauthority if unset */
  if (g_getenv ("ICEAUTHORITY"))
    authfile = g_strdup (g_getenv ("ICEAUTHORITY"));
  else
    authfile = expidus_get_homefile (".ICEauthority", NULL);
  fd = open (authfile, O_RDWR | O_CREAT, 0600);
  if (fd < 0)
    {
      fprintf (stderr, "expidus1-session: Unable to access file %s: %s\n",
               authfile, g_strerror (errno));
      exit (EXIT_FAILURE);
    }
  g_free (authfile);
  close (fd);
}

static void
init_display (EssmManager   *manager,
              GdkDisplay    *dpy,
              gboolean       disable_tcp)
{
  gdk_flush ();

  sm_init (channel, disable_tcp, manager);
}



static void
manager_quit_cb (EssmManager *manager, gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);

  g_bus_unown_name (name_id);

  name_lost (connection, "expidus1-session", manager);
}



static void
bus_acquired (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
  EssmManager      *manager = ESSM_MANAGER (user_data);
  GdkDisplay       *dpy;

  essm_verbose ("bus_acquired %s\n", name);

  manager = essm_manager_new (connection);

  if (manager == NULL) {
          g_critical ("Could not create EssmManager");
          return;
  }

  g_signal_connect(G_OBJECT(manager),
                   "manager-quit",
                   G_CALLBACK(manager_quit_cb),
                   connection);

  setup_environment ();

  channel = essm_open_config ();

  dpy = gdk_display_get_default ();
  init_display (manager, dpy, opt_disable_tcp);

  if (!opt_disable_tcp && esconf_channel_get_bool (channel, "/security/EnableTcp", FALSE))
    {
      /* verify that the DNS settings are ok */      
      essm_dns_check ();
    }


  essm_startup_init (channel);
  essm_manager_load (manager, channel);
  essm_manager_restart (manager);
}



static void
name_acquired (GDBusConnection *connection,
               const gchar *name,
               gpointer user_data)
{
  essm_verbose ("name_acquired\n");
}



static void
name_lost (GDBusConnection *connection,
           const gchar *name,
           gpointer user_data)
{
  EssmManager      *manager = ESSM_MANAGER (user_data);
  GError           *error = NULL;
  EssmShutdownType  shutdown_type;
  EssmShutdown     *shutdown_helper;
  gboolean          succeed = TRUE;

  essm_verbose ("name_lost\n");

  /* Release the  object */
  essm_verbose ("Disconnected from D-Bus");

  shutdown_type = essm_manager_get_shutdown_type (manager);

  /* take over the ref before we release the manager */
  shutdown_helper = essm_shutdown_get ();

  ice_cleanup ();

  if (shutdown_type == ESSM_SHUTDOWN_SHUTDOWN
      || shutdown_type == ESSM_SHUTDOWN_RESTART)
    {
      succeed = essm_shutdown_try_type (shutdown_helper, shutdown_type, &error);
      if (!succeed)
        g_warning ("Failed to shutdown/restart: %s", ERROR_MSG (error));
    }

  g_object_unref (shutdown_helper);
  g_object_unref (manager);
  g_object_unref (channel);
  g_clear_error (&error);

  shutdown_helper = NULL;
  manager = NULL;
  channel = NULL;

  gtk_main_quit ();
}



static void
essm_dbus_init (EssmManager *manager)
{
  name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                            "org.expidus.SessionManager",
                            G_BUS_NAME_OWNER_FLAGS_NONE,
                            bus_acquired, name_acquired, name_lost,
                            manager,
                            NULL);

  if (name_id == 0)
    {
      g_printerr ("%s: Another session manager is already running\n", PACKAGE_NAME);
      exit (EXIT_FAILURE);
    }
}



static gboolean
essm_dbus_require_session (gint argc, gchar **argv)
{
  gchar **new_argv;
  gchar  *path;
  gint    i;
  guint   m = 0;

  if (g_getenv ("DBUS_SESSION_BUS_ADDRESS") != NULL)
    return TRUE;

  path = g_find_program_in_path ("dbus-launch");
  if (path == NULL)
    {
      g_critical ("dbus-launch not found, the desktop will not work properly!");
      return TRUE;
    }

  /* avoid rondtrips */
  g_assert (!g_str_has_prefix (*argv, "dbus-launch"));

  new_argv = g_new0 (gchar *, argc + 4);
  new_argv[m++] = path;
  new_argv[m++] = "--sh-syntax";
  new_argv[m++] = "--exit-with-session";

  for (i = 0; i < argc; i++)
    new_argv[m++] = argv[i];

  if (!execvp ("dbus-launch", new_argv))
    {
      g_critical ("Could not spawn %s: %s", path, g_strerror (errno));
    }

  g_free (path);
  g_free (new_argv);

  return FALSE;
}

int
main (int argc, char **argv)
{
  EssmManager      *manager = NULL;
  GError           *error = NULL;

  if (!essm_dbus_require_session (argc, argv))
    return EXIT_SUCCESS;

  expidus_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* install required signal handlers */
  signal (SIGPIPE, SIG_IGN);

  if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
      g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
      g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
      g_print ("\n");
      g_error_free (error);
      return EXIT_FAILURE;
    }

  if (opt_version)
    {
      g_print ("%s %s (Expidus %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, expidus_version_string ());
      g_print ("%s\n", "Copyright (c) 2003-2020");
      g_print ("\t%s\n\n", _("The Expidus development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (!esconf_init (&error))
    {
      expidus_dialog_show_error (NULL, error, _("Unable to contact settings server"));
      g_error_free (error);
    }

  /* Process all pending events prior to start DBUS */
  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* fake a client id for the manager, so the legacy management does not
   * recognize us to be a session client.
   */
  gdk_x11_set_sm_client_id (essm_generate_client_id (NULL));

  essm_dbus_init (manager);

  gtk_main ();

  essm_startup_shutdown ();

  return EXIT_SUCCESS;
}
