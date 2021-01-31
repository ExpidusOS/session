/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@expidus.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@expidus.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
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

#include <gio/gio.h>

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libwnck/libwnck.h>

#include <libexpidus1ui/libexpidus1ui.h>

#include <libessm/essm-util.h>

#include <expidus1-session/essm-manager-dbus.h>
#include <expidus1-session/essm-manager.h>
#include <expidus1-session/essm-chooser-icon.h>
#include <expidus1-session/essm-chooser.h>
#include <expidus1-session/essm-global.h>
#include <expidus1-session/essm-legacy.h>
#include <expidus1-session/essm-startup.h>
#include <expidus1-session/essm-marshal.h>
#include <expidus1-session/essm-error.h>
#include <expidus1-session/essm-logout-dialog.h>


struct _EssmManager
{
  EssmDbusManagerSkeleton parent;

  EssmManagerState state;

  EssmShutdownType  shutdown_type;
  EssmShutdown     *shutdown_helper;
  gboolean          save_session;

  gboolean         session_chooser;
  gchar           *session_name;
  gchar           *session_file;
  gchar           *checkpoint_session_name;

  gboolean         start_at;

  gboolean         compat_gnome;
  gboolean         compat_kde;

  GQueue          *starting_properties;
  GQueue          *pending_properties;
  GQueue          *restart_properties;
  GQueue          *running_clients;

  gboolean         failsafe_mode;
  gint             failsafe_clients_pending;

  guint            die_timeout_id;
  guint            name_owner_id;

  GDBusConnection *connection;
};

typedef struct _EssmManagerClass
{
  EssmDbusManagerSkeletonClass parent;

  /*< signals >*/
  void (*quit)(EssmManager *manager);
} EssmManagerClass;

typedef struct
{
  EssmManager *manager;
  EssmClient  *client;
  guint        timeout_id;
} EssmSaveTimeoutData;

typedef struct
{
  EssmManager     *manager;
  EssmShutdownType type;
  gboolean         allow_save;
} ShutdownIdleData;

enum
{
    MANAGER_QUIT,
    LAST_SIGNAL,
};

static guint manager_signals[LAST_SIGNAL] = { 0, };

int               essm_splash_screen_choose (GList            *sessions,
                                             const gchar      *default_session,
                                             gchar           **name_return);
static void       essm_manager_finalize (GObject *obj);

static void       essm_manager_start_client_save_timeout (EssmManager *manager,
                                                          EssmClient  *client);
static void       essm_manager_cancel_client_save_timeout (EssmManager *manager,
                                                           EssmClient  *client);
static gboolean   essm_manager_save_timeout (gpointer user_data);
static void       essm_manager_load_settings (EssmManager   *manager,
                                              EsconfChannel *channel);
static gboolean   essm_manager_load_session (EssmManager *manager,
                                             EsconfChannel *channel);
static void       essm_manager_dbus_class_init (EssmManagerClass *klass);
static void       essm_manager_dbus_init (EssmManager *manager,
                                          GDBusConnection *connection);
static void       essm_manager_iface_init (EssmDbusManagerIface *iface);
static void       essm_manager_dbus_cleanup (EssmManager *manager);



G_DEFINE_TYPE_WITH_CODE (EssmManager, essm_manager, ESSM_DBUS_TYPE_MANAGER_SKELETON, G_IMPLEMENT_INTERFACE (ESSM_DBUS_TYPE_MANAGER, essm_manager_iface_init));


static void
essm_manager_class_init (EssmManagerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = essm_manager_finalize;

  manager_signals[MANAGER_QUIT] = g_signal_new("manager-quit",
                                               G_OBJECT_CLASS_TYPE(gobject_class),
                                               G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET(EssmManagerClass, quit),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

  essm_manager_dbus_class_init (klass);
}


static void
essm_manager_init (EssmManager *manager)
{
  manager->state = ESSM_MANAGER_STARTUP;
  manager->session_chooser = FALSE;
  manager->failsafe_mode = TRUE;
  manager->shutdown_type = ESSM_SHUTDOWN_LOGOUT;
  manager->shutdown_helper = essm_shutdown_get ();
  manager->save_session = TRUE;

  manager->pending_properties = g_queue_new ();
  manager->starting_properties = g_queue_new ();
  manager->restart_properties = g_queue_new ();
  manager->running_clients = g_queue_new ();
  manager->failsafe_clients_pending = 0;
}

static void
essm_manager_finalize (GObject *obj)
{
  EssmManager *manager = ESSM_MANAGER(obj);

  essm_manager_dbus_cleanup (manager);

  if (manager->die_timeout_id != 0)
    g_source_remove (manager->die_timeout_id);

  g_object_unref (manager->shutdown_helper);

  g_queue_foreach (manager->pending_properties, (GFunc) G_CALLBACK (essm_properties_free), NULL);
  g_queue_free (manager->pending_properties);

  g_queue_foreach (manager->starting_properties, (GFunc) G_CALLBACK (essm_properties_free), NULL);
  g_queue_free (manager->starting_properties);

  g_queue_foreach (manager->restart_properties, (GFunc) G_CALLBACK (essm_properties_free), NULL);
  g_queue_free (manager->restart_properties);

  g_queue_foreach (manager->running_clients, (GFunc) G_CALLBACK (g_object_unref), NULL);
  g_queue_free (manager->running_clients);

  g_free (manager->session_name);
  g_free (manager->session_file);
  g_free (manager->checkpoint_session_name);

  G_OBJECT_CLASS (essm_manager_parent_class)->finalize (obj);
}


#ifdef G_CAN_INLINE
static inline void
#else
static void
#endif
essm_manager_set_state (EssmManager     *manager,
                        EssmManagerState state)
{
  EssmManagerState old_state;

  /* idea here is to use this to set state always so we don't forget
   * to emit the signal */

  if (state == manager->state)
    return;

  old_state = manager->state;
  manager->state = state;

  essm_verbose ("\nstate is now %s\n",
                state == ESSM_MANAGER_STARTUP ? "ESSM_MANAGER_STARTUP" :
                state == ESSM_MANAGER_IDLE ?  "ESSM_MANAGER_IDLE" :
                state == ESSM_MANAGER_CHECKPOINT ? "ESSM_MANAGER_CHECKPOINT" :
                state == ESSM_MANAGER_SHUTDOWN ? "ESSM_MANAGER_SHUTDOWN" :
                state == ESSM_MANAGER_SHUTDOWNPHASE2 ? "ESSM_MANAGER_SHUTDOWNPHASE2" :
                "unknown");

  essm_dbus_manager_emit_state_changed (ESSM_DBUS_MANAGER (manager), old_state, state);
}


EssmManager *
essm_manager_new (GDBusConnection *connection)
{
  EssmManager *manager = ESSM_MANAGER (g_object_new (ESSM_TYPE_MANAGER, NULL));

  essm_manager_dbus_init (manager, connection);

  return manager;
}


static gboolean
essm_manager_startup (gpointer user_data)
{
  EssmManager *manager = user_data;

  essm_startup_foreign (manager);
  g_queue_sort (manager->pending_properties, (GCompareDataFunc) G_CALLBACK (essm_properties_compare), NULL);
  essm_startup_begin (manager);
  return FALSE;
}


static void
essm_manager_restore_active_workspace (EssmManager *manager,
                                       ExpidusRc      *rc)
{
  WnckWorkspace  *workspace;
  GdkDisplay     *display;
  WnckScreen     *screen;
  gchar           buffer[1024];
  gint            n, m;

  display = gdk_display_get_default ();
  for (n = 0; n < XScreenCount (gdk_x11_display_get_xdisplay (display)); ++n)
    {
      g_snprintf (buffer, 1024, "Screen%d_ActiveWorkspace", n);
      essm_verbose ("Attempting to restore %s\n", buffer);
      if (!expidus_rc_has_entry (rc, buffer))
        {
          essm_verbose ("no entry found\n");
          continue;
        }

      m = expidus_rc_read_int_entry (rc, buffer, 0);

      screen = wnck_screen_get (n);
      wnck_screen_force_update (screen);

      if (wnck_screen_get_workspace_count (screen) > m)
        {
          workspace = wnck_screen_get_workspace (screen, m);
          wnck_workspace_activate (workspace, GDK_CURRENT_TIME);
        }
    }
}


gboolean
essm_manager_handle_failed_properties (EssmManager    *manager,
                                       EssmProperties *properties)
{
  gint restart_style_hint;
  GError *error = NULL;

  /* Handle apps that failed to start, or died randomly, here */

  essm_properties_set_default_child_watch (properties);

  if (properties->restart_attempts_reset_id > 0)
    {
      g_source_remove (properties->restart_attempts_reset_id);
      properties->restart_attempts_reset_id = 0;
    }

  restart_style_hint = essm_properties_get_uchar (properties,
                                                  SmRestartStyleHint,
                                                  SmRestartIfRunning);

  if (restart_style_hint == SmRestartAnyway)
    {
      essm_verbose ("Client id %s died or failed to start, restarting anyway\n", properties->client_id);
      g_queue_push_tail (manager->restart_properties, properties);
    }
  else if (restart_style_hint == SmRestartImmediately)
    {
      if (++properties->restart_attempts > MAX_RESTART_ATTEMPTS)
        {
          essm_verbose ("Client Id = %s, reached maximum attempts [Restart attempts = %d]\n"
                        "   Will be re-scheduled for run on next startup\n",
                        properties->client_id, properties->restart_attempts);

          g_queue_push_tail (manager->restart_properties, properties);
        }
      else
        {
          essm_verbose ("Client Id = %s disconnected, restarting\n",
                        properties->client_id);

          if (G_UNLIKELY (!essm_startup_start_properties (properties, manager)))
            {
              /* this failure has nothing to do with the app itself, so
               * just add it to restart props */
              g_queue_push_tail (manager->restart_properties, properties);
            }
          else
            {
              /* put it back in the starting list */
              g_queue_push_tail (manager->starting_properties, properties);
            }
        }
    }
  else
    {
      gchar **discard_command;

      /* We get here if a SmRestartNever or SmRestartIfRunning client
       * has exited.  SmRestartNever clients shouldn't have discard
       * commands, but it can't hurt to run it if it has one for some
       * reason, and might clean up garbage we don't want. */
      essm_verbose ("Client Id %s exited, removing from session.\n",
                    properties->client_id);

      discard_command = essm_properties_get_strv (properties, SmDiscardCommand);
      if (discard_command != NULL && g_strv_length (discard_command) > 0)
        {
          /* Run the SmDiscardCommand after the client exited in any state,
           * but only if we don't expect the client to be restarted,
           * whether immediately or in the next session.
           *
           * NB: This used to also have the condition that the manager is
           * in the IDLE state, but this was removed because I can't see
           * why you'd treat a client that fails during startup any
           * differently, and this fixes a potential properties leak.
           *
           * Unfortunately the spec isn't clear about the usage of the
           * discard command. Have to check ksmserver/gnome-session, and
           * come up with consistent behaviour.
           *
           * But for now, this work-around fixes the problem of the
           * ever-growing number of eswm1 session files when restarting
           * eswm1 within a session.
           */
          essm_verbose ("Client Id = %s: running discard command %s:%d.\n\n",
                        properties->client_id, *discard_command,
                        g_strv_length (discard_command));

          if (!g_spawn_sync (essm_properties_get_string (properties, SmCurrentDirectory),
                             discard_command,
                             essm_properties_get_strv (properties, SmEnvironment),
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL,
                             NULL, NULL,
                             NULL, &error))
            {
              g_warning ("Failed to running discard command \"%s\": %s",
                         *discard_command, error->message);
              g_error_free (error);
            }
        }

      return FALSE;
    }

  return TRUE;
}


static gboolean
essm_manager_choose_session (EssmManager *manager,
                             ExpidusRc      *rc)
{
  EssmSessionInfo *session;
  gboolean         load = FALSE;
  GList           *sessions = NULL;
  GList           *lp;
  gchar           *name;
  gint             result;

  sessions = settings_list_sessions (rc);

  if (sessions != NULL)
    {
      result = essm_splash_screen_choose (sessions,
                                          manager->session_name, &name);

      if (result == ESSM_CHOOSE_LOGOUT)
        {
          expidus_rc_close (rc);
          exit (EXIT_SUCCESS);
        }
      else if (result == ESSM_CHOOSE_LOAD)
        {
          load = TRUE;
        }

      if (manager->session_name != NULL)
        g_free (manager->session_name);
      manager->session_name = name;

      for (lp = sessions; lp != NULL; lp = lp->next)
        {
          session = (EssmSessionInfo *) lp->data;
          g_object_unref (session->preview);
          g_free (session);
        }

      g_list_free (sessions);
    }

  return load;
}


static gboolean
essm_manager_load_session (EssmManager   *manager,
                           EsconfChannel *channel)
{
  EssmProperties *properties;
  gchar           buffer[1024];
  ExpidusRc         *rc;
  gint            count;

  if (!g_file_test (manager->session_file, G_FILE_TEST_IS_REGULAR))
    {
      g_debug ("essm_manager_load_session: Something wrong with %s, Does it exist? Permissions issue?", manager->session_file);
      return FALSE;
    }

  rc = expidus_rc_simple_open (manager->session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
  {
    g_warning ("essm_manager_load_session: unable to open %s", manager->session_file);
    return FALSE;
  }

  if (manager->session_chooser && !essm_manager_choose_session (manager, rc))
    {
      g_warning ("essm_manager_load_session: failed to choose session");
      expidus_rc_close (rc);
      return FALSE;
    }

  g_snprintf (buffer, 1024, "Session: %s", manager->session_name);
  essm_verbose ("loading %s\n", buffer);
  esconf_channel_set_string (channel, "/general/SessionName", manager->session_name);

  expidus_rc_set_group (rc, buffer);
  count = expidus_rc_read_int_entry (rc, "Count", 0);
  if (G_UNLIKELY (count <= 0))
    {
      expidus_rc_close (rc);
      return FALSE;
    }

  while (count-- > 0)
    {
      g_snprintf (buffer, 1024, "Client%d_", count);
      properties = essm_properties_load (rc, buffer);
      if (G_UNLIKELY (properties == NULL))
        {
          essm_verbose ("%s has no properties. Skipping\n", buffer);
          continue;
        }
      if (essm_properties_check (properties))
        {
          g_queue_push_tail (manager->pending_properties, properties);
        }
      else
        {
          essm_verbose ("%s has invalid properties. Skipping\n", buffer);
          essm_properties_free (properties);
        }
    }

  essm_verbose ("Finished loading clients from rc file\n");

  /* load legacy applications */
  essm_legacy_load_session (rc);

  expidus_rc_close (rc);

  return g_queue_peek_head (manager->pending_properties) != NULL;
}


static gboolean
essm_manager_load_failsafe (EssmManager   *manager,
                            EsconfChannel *channel,
                            gchar        **error)
{
  EssmProperties *properties;
  gchar          *failsafe_name;
  gchar           propbuf[4096];
  gchar          *hostname;
  gchar          *client_id = NULL;
  gchar         **command;
  gchar           command_entry[256];
  gchar           priority_entry[256];
  gint            priority;
  gint            count;
  gint            i;

  hostname = expidus_gethostname ();

  failsafe_name = esconf_channel_get_string (channel, "/general/FailsafeSessionName", NULL);
  if (G_UNLIKELY (!failsafe_name))
    {
      if (error)
        *error = g_strdup_printf (_("Unable to determine failsafe session name.  Possible causes: esconfd isn't running (D-Bus setup problem); environment variable $XDG_CONFIG_DIRS is set incorrectly (must include \"%s\"), or expidus1-session is installed incorrectly."),
                                  SYSCONFDIR);
      return FALSE;
    }

  g_snprintf (propbuf, sizeof (propbuf), "/sessions/%s/IsFailsafe",
              failsafe_name);
  if (!esconf_channel_get_bool (channel, propbuf, FALSE))
    {
      if (error)
        {
          *error = g_strdup_printf (_("The specified failsafe session (\"%s\") is not marked as a failsafe session."),
                                    failsafe_name);
        }
      g_free (failsafe_name);
      return FALSE;
    }

  g_snprintf (propbuf, sizeof (propbuf), "/sessions/%s/Count", failsafe_name);
  count = esconf_channel_get_int (channel, propbuf, 0);

  for (i = 0; i < count; ++i)
    {
      properties = essm_properties_new (client_id, hostname);
      g_snprintf (command_entry, sizeof (command_entry),
                  "/sessions/%s/Client%d_Command", failsafe_name, i);
      command = esconf_channel_get_string_list (channel, command_entry);
      if (G_UNLIKELY (command == NULL))
        continue;

      g_snprintf (priority_entry, sizeof (priority_entry),
                  "/sessions/%s/Client%d_Priority", failsafe_name, i);
      priority = esconf_channel_get_int (channel, priority_entry, 50);

      essm_properties_set_string (properties, SmProgram, command[0]);
      essm_properties_set_strv (properties, SmRestartCommand, g_strdupv(command));
      essm_properties_set_uchar (properties, GsmPriority, priority);
      g_queue_push_tail (manager->pending_properties, properties);
    }
  g_queue_sort (manager->pending_properties, (GCompareDataFunc) G_CALLBACK (essm_properties_compare), NULL);

  if (g_queue_peek_head (manager->pending_properties) == NULL)
    {
      if (error)
        *error = g_strdup (_("The list of applications in the failsafe session is empty."));
      return FALSE;
    }

  return TRUE;
}


int
essm_splash_screen_choose (GList            *sessions,
                           const gchar      *default_session,
                           gchar           **name_return)
{
  GdkDisplay *display;
  GdkScreen  *screen;
  int         monitor;
  GtkWidget  *chooser;
  GtkWidget  *label;
  GtkWidget  *dialog;
  GtkWidget  *entry;
  gchar       title[256];
  int         result;

  g_assert (default_session != NULL);


again:
      display = gdk_display_get_default ();
      screen = expidus_gdk_screen_get_active (&monitor);

      if (G_UNLIKELY (screen == NULL))
        {
          screen  = gdk_display_get_default_screen (display);
          monitor = 0;
        }

      chooser = g_object_new (ESSM_TYPE_CHOOSER,
                              "type", GTK_WINDOW_POPUP,
                              NULL);
      gtk_container_set_border_width (GTK_CONTAINER (chooser), 6);
      essm_chooser_set_sessions (ESSM_CHOOSER (chooser),
                                 sessions, default_session);
      gtk_window_set_screen (GTK_WINDOW (chooser), screen);
      gtk_window_set_position (GTK_WINDOW (chooser), GTK_WIN_POS_CENTER);
      /* Use Adwaita's keycap style to get a meaningful look out of the box */
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (chooser)), "keycap");
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (chooser)), "essm-session-chooser");
      result = gtk_dialog_run (GTK_DIALOG (chooser));

      if (result == ESSM_RESPONSE_LOAD)
        {
          if (name_return != NULL)
            *name_return = essm_chooser_get_session (ESSM_CHOOSER (chooser));
          result = ESSM_CHOOSE_LOAD;
        }
      else if (result == ESSM_RESPONSE_NEW)
        {
          result = ESSM_CHOOSE_NEW;
        }
      else
        {
          result = ESSM_CHOOSE_LOGOUT;
        }

      gtk_widget_destroy (chooser);

      if (result == ESSM_CHOOSE_NEW)
        {
          GtkWidget *button;
          dialog = gtk_dialog_new_with_buttons (NULL,
                                                NULL,
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                _("_Cancel"),
                                                GTK_RESPONSE_CANCEL,
                                                _("_OK"),
                                                GTK_RESPONSE_OK,
                                                NULL);
          gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK);
          gtk_window_set_screen (GTK_WINDOW (dialog), screen);
          gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
          gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
          /* Use Adwaita's keycap style to get a meaningful look out of the box */
          gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (dialog)), "keycap");
          gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (dialog)), "essm-session-chooser");
          g_snprintf (title, 256, "<big><b>%s</b></big>",
                      _("Name for the new session"));
          label = gtk_label_new (title);
          gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
          gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                              label, TRUE, TRUE, 6);
          gtk_widget_show (label);

          button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                       GTK_RESPONSE_OK);
          gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");

          entry = gtk_entry_new ();
          gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
          gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                              entry, TRUE, TRUE, 6);
          gtk_widget_show (entry);

again1:
          result = gtk_dialog_run (GTK_DIALOG (dialog));

          if (result != GTK_RESPONSE_OK)
            {
              gtk_widget_destroy (dialog);
              goto again;
            }

          if (name_return != NULL)
            {
              *name_return = gtk_editable_get_chars (GTK_EDITABLE (entry),
                                                     0, -1);
              if (strlen (*name_return) == 0)
                {
                  g_free (*name_return);
                  goto again1;
                }
            }

          gtk_widget_destroy (dialog);
          result = ESSM_CHOOSE_NEW;
        }

  return result;
}


static void
essm_manager_load_settings (EssmManager   *manager,
                            EsconfChannel *channel)
{
  gboolean session_loaded = FALSE;

  manager->session_name = esconf_channel_get_string (channel,
                                                     "/general/SessionName",
                                                     DEFAULT_SESSION_NAME);
  if (G_UNLIKELY (manager->session_name[0] == '\0'))
    {
      g_free (manager->session_name);
      manager->session_name = g_strdup (DEFAULT_SESSION_NAME);
    }

  manager->session_chooser = esconf_channel_get_bool (channel, "/chooser/AlwaysDisplay", FALSE);

  session_loaded = essm_manager_load_session (manager, channel);

  if (session_loaded)
    {
      essm_verbose ("Session \"%s\" loaded successfully.\n\n", manager->session_name);
      manager->failsafe_mode = FALSE;
    }
  else
    {
      gchar *errorstr = NULL;

      essm_verbose ("Loading failsafe session\n");
      if (!essm_manager_load_failsafe (manager, channel, &errorstr))
        {
          /* FIXME: migrate this into the splash screen somehow so the
           * window doesn't look ugly (right now no WM is running, so it
           * won't have window decorations). */
          expidus_message_dialog (NULL, _("Session Manager Error"),
                               "dialog-error",
                               _("Unable to load a failsafe session"),
                               errorstr,
                               EXPIDUS_BUTTON_TYPE_MIXED, "application-exit", _("_Quit"), GTK_RESPONSE_ACCEPT, NULL);
          g_free (errorstr);
          exit (EXIT_FAILURE);
        }
      manager->failsafe_mode = TRUE;
    }
}


void
essm_manager_load (EssmManager   *manager,
                   EsconfChannel *channel)
{
  gchar *display_name;
  gchar *resource_name;
#ifdef HAVE_OS_CYGWIN
  gchar *s;
#endif

  manager->compat_gnome = esconf_channel_get_bool (channel, "/compat/LaunchGNOME", FALSE);
  manager->compat_kde = esconf_channel_get_bool (channel, "/compat/LaunchKDE", FALSE);
  manager->start_at = esconf_channel_get_bool (channel, "/general/StartAssistiveTechnologies", FALSE);

  display_name  = essm_gdk_display_get_fullname (gdk_display_get_default ());

#ifdef HAVE_OS_CYGWIN
  /* rename a colon (:) to a hash (#) under cygwin. windows doesn't like
   * filenames with a colon... */
  for (s = display_name; *s != '\0'; ++s)
    if (*s == ':')
      *s = '#';
#endif

  resource_name = g_strconcat ("sessions/expidus1-session-", display_name, NULL);
  manager->session_file  = expidus_resource_save_location (EXPIDUS_RESOURCE_CACHE, resource_name, TRUE);
  g_free (resource_name);
  g_free (display_name);

  essm_manager_load_settings (manager, channel);
}


gboolean
essm_manager_restart (EssmManager *manager)
{
  g_assert (manager->session_name != NULL);

  /* setup legacy application handling */
  essm_legacy_init ();

  g_idle_add (essm_manager_startup, manager);

  return TRUE;
}


void
essm_manager_signal_startup_done (EssmManager *manager)
{
  gchar buffer[1024];
  ExpidusRc *rc;

  essm_verbose ("Manager finished startup, entering IDLE mode now\n\n");
  essm_manager_set_state (manager, ESSM_MANAGER_IDLE);

  if (!manager->failsafe_mode)
    {
      /* restore active workspace, this has to be done after the
       * window manager is up, so we do it last.
       */
      g_snprintf (buffer, 1024, "Session: %s", manager->session_name);
      rc = expidus_rc_simple_open (manager->session_file, TRUE);
      expidus_rc_set_group (rc, buffer);
      essm_manager_restore_active_workspace (manager, rc);
      expidus_rc_close (rc);

      /* start legacy applications now */
      essm_legacy_startup ();
    }
}


EssmClient*
essm_manager_new_client (EssmManager *manager,
                         SmsConn      sms_conn,
                         gchar      **error)
{
  EssmClient *client = NULL;

  if (G_UNLIKELY (manager->state != ESSM_MANAGER_IDLE)
      && G_UNLIKELY (manager->state != ESSM_MANAGER_STARTUP))
    {
      if (error != NULL)
        *error = "We don't accept clients while in CheckPoint/Shutdown state!";
      return NULL;
    }

  client = essm_client_new (manager, sms_conn, manager->connection);
  return client;
}


static gboolean
essm_manager_reset_restart_attempts (gpointer data)
{
  EssmProperties *properties = data;

  properties->restart_attempts = 0;
  properties->restart_attempts_reset_id = 0;

  return FALSE;
}


static EssmProperties*
essm_manager_get_pending_properties (EssmManager *manager,
                                     const gchar *previous_id)
{
  EssmProperties *properties = NULL;
  GList          *lp;

  lp = g_queue_find_custom (manager->starting_properties,
                            previous_id,
                            (GCompareFunc) essm_properties_compare_id);

  if (lp != NULL)
    {
      properties = ESSM_PROPERTIES (lp->data);
      g_queue_delete_link (manager->starting_properties, lp);
    }
  else
    {
      lp = g_queue_find_custom (manager->pending_properties,
                                previous_id,
                                (GCompareFunc) essm_properties_compare_id);
      if (lp != NULL)
        {
          properties = ESSM_PROPERTIES (lp->data);
          g_queue_delete_link (manager->pending_properties, lp);
        }
    }

  return properties;
}

static void
essm_manager_handle_old_client_reregister (EssmManager    *manager,
                                           EssmClient     *client,
                                           EssmProperties *properties)
{
  /* cancel startup timer */
  if (properties->startup_timeout_id > 0)
    {
      g_source_remove (properties->startup_timeout_id);
      properties->startup_timeout_id = 0;
    }

  /* cancel the old child watch, and replace it with one that
   * doesn't really do anything but reap the child */
  essm_properties_set_default_child_watch (properties);

  essm_client_set_initial_properties (client, properties);

  /* if we've been restarted, we'll want to reset the restart
   * attempts counter if the client stays alive for a while */
  if (properties->restart_attempts > 0 && properties->restart_attempts_reset_id == 0)
    {
      properties->restart_attempts_reset_id = g_timeout_add (RESTART_RESET_TIMEOUT,
                                                             essm_manager_reset_restart_attempts,
                                                             properties);
    }
}

gboolean
essm_manager_register_client (EssmManager *manager,
                              EssmClient  *client,
                              const gchar *dbus_client_id,
                              const gchar *previous_id)
{
  EssmProperties *properties = NULL;
  SmsConn         sms_conn;

  sms_conn = essm_client_get_sms_connection (client);

  /* This part is for sms based clients */
  if (previous_id != NULL)
    {
      properties = essm_manager_get_pending_properties (manager, previous_id);

      /* If previous_id is invalid, the SM will send a BadValue error message
       * to the client and reverts to register state waiting for another
       * RegisterClient message.
       */
      if (properties == NULL)
        {
          essm_verbose ("Client Id = %s registering, failed to find matching "
                        "properties\n", previous_id);
          return FALSE;
        }

      essm_manager_handle_old_client_reregister (manager, client, properties);
    }
  else
    {
      essm_verbose ("No previous_id found.\n");
      if (manager->failsafe_mode)
        essm_verbose ("Plus, we're obviously running in failsafe mode.\n");
      if (sms_conn != NULL)
        {
          /* new sms client */
          gchar *client_id = essm_generate_client_id (sms_conn);

          properties = essm_properties_new (client_id, SmsClientHostName (sms_conn));
          essm_client_set_initial_properties (client, properties);

          g_free (client_id);
        }
    }

  /* this part is for dbus clients */
  if (dbus_client_id != NULL)
    {
      essm_verbose ("dbus_client_id found: %s.\n", dbus_client_id);
      if (manager->failsafe_mode)
        essm_verbose ("Plus, we're obviously running in failsafe mode.\n");
      properties = essm_manager_get_pending_properties (manager, dbus_client_id);

      if (properties != NULL)
        {
          essm_manager_handle_old_client_reregister (manager, client, properties);
          /* need this to continue session loading during ESSM_MANAGER_STARTUP */
          previous_id = dbus_client_id;
        }
      else
        {
          /* new dbus client */
          gchar *hostname = expidus_gethostname ();

          properties = essm_properties_new (dbus_client_id, hostname);
          essm_client_set_initial_properties (client, properties);

          g_free (hostname);
        }
    }
  else
    essm_verbose ("No dbus_client_id found.\n");


  g_queue_push_tail (manager->running_clients, client);

  if (sms_conn != NULL)
    {
      SmsRegisterClientReply (sms_conn, (char *) essm_client_get_id (client));
    }

  essm_dbus_manager_emit_client_registered (ESSM_DBUS_MANAGER (manager), essm_client_get_object_path (client));

  if (previous_id == NULL)
    {
      if (sms_conn != NULL)
        {
          SmsSaveYourself (sms_conn, SmSaveLocal, False, SmInteractStyleNone, False);
          essm_client_set_state (client, ESSM_CLIENT_SAVINGLOCAL);
          essm_manager_start_client_save_timeout (manager, client);
        }
    }

  if ((previous_id != NULL || manager->failsafe_mode)
       && manager->state == ESSM_MANAGER_STARTUP)
    {
      /* Only continue the startup if the previous_id matched one of
       * the starting_properties. If there was no match above,
       * previous_id will be NULL here.  We don't need to continue when
       * in failsafe mode because in that case the failsafe session is
       * started all at once.
       */
      if (manager->failsafe_mode)
        {
          if (--manager->failsafe_clients_pending == 0)
            g_queue_clear (manager->starting_properties);
        }
      if (g_queue_peek_head (manager->starting_properties) == NULL)
        essm_startup_session_continue (manager);
    }

  return TRUE;
}


void
essm_manager_start_interact (EssmManager *manager,
                             EssmClient  *client)
{
  SmsConn sms = essm_client_get_sms_connection (client);

  /* notify client of interact */
  if (sms != NULL)
    {
      SmsInteract (sms);
    }
  essm_client_set_state (client, ESSM_CLIENT_INTERACTING);

  /* stop save yourself timeout */
  essm_manager_cancel_client_save_timeout (manager, client);
}


void
essm_manager_interact (EssmManager *manager,
                       EssmClient  *client,
                       int          dialog_type)
{
  GList *lp;

  if (G_UNLIKELY (essm_client_get_state (client) != ESSM_CLIENT_SAVING))
    {
      essm_verbose ("Client Id = %s, requested INTERACT, but client is not in SAVING mode\n"
                    "   Client will be disconnected now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
    }
  else if (G_UNLIKELY (manager->state != ESSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (manager->state != ESSM_MANAGER_SHUTDOWN))
    {
      essm_verbose ("Client Id = %s, requested INTERACT, but manager is not in CheckPoint/Shutdown mode\n"
                    "   Client will be disconnected now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
    }
  else
    {
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          EssmClient *cl = lp->data;
          if (essm_client_get_state (cl) == ESSM_CLIENT_INTERACTING)
            {
              /* a client is already interacting, so new client has to wait */
              essm_client_set_state (client, ESSM_CLIENT_WAITFORINTERACT);
              essm_manager_cancel_client_save_timeout(manager, client);
              return;
            }
        }

      essm_manager_start_interact (manager, client);
    }
}


void
essm_manager_interact_done (EssmManager *manager,
                            EssmClient  *client,
                            gboolean     cancel_shutdown)
{
  GList *lp;

  if (G_UNLIKELY (essm_client_get_state (client) != ESSM_CLIENT_INTERACTING))
    {
      essm_verbose ("Client Id = %s, send INTERACT DONE, but client is not in INTERACTING state\n"
                    "   Client will be disconnected now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
      return;
    }
  else if (G_UNLIKELY (manager->state != ESSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (manager->state != ESSM_MANAGER_SHUTDOWN))
    {
      essm_verbose ("Client Id = %s, send INTERACT DONE, but manager is not in CheckPoint/Shutdown state\n"
                    "   Client will be disconnected now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
      return;
    }

  essm_client_set_state (client, ESSM_CLIENT_SAVING);

  /* Setting the cancel-shutdown field to True indicates that the user
   * has requested that the entire shutdown be cancelled. Cancel-
   * shutdown may only be True if the corresponding SaveYourself
   * message specified True for the shutdown field and Any or Error
   * for the interact-style field. Otherwise, cancel-shutdown must be
   * False.
   */
  if (cancel_shutdown && manager->state == ESSM_MANAGER_SHUTDOWN)
    {
      /* we go into checkpoint state from here... */
      essm_manager_set_state (manager, ESSM_MANAGER_CHECKPOINT);

      /* If a shutdown is in progress, the user may have the option
       * of cancelling the shutdown. If the shutdown is cancelled
       * (specified in the "Interact Done" message), the session
       * manager should send a "Shutdown Cancelled" message to each
       * client that requested to interact.
       */
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          EssmClient *cl = lp->data;
          SmsConn sms = essm_client_get_sms_connection (cl);

          /* only sms clients do the interact stuff */
          if (sms && essm_client_get_state (cl) != ESSM_CLIENT_WAITFORINTERACT)
            continue;

          /* reset all clients that are waiting for interact */
          essm_client_set_state (client, ESSM_CLIENT_SAVING);
          if (sms != NULL)
            {
              SmsShutdownCancelled (sms);
            }
          else
            {
              essm_client_cancel_shutdown (client);
            }
        }

        essm_dbus_manager_emit_shutdown_cancelled (ESSM_DBUS_MANAGER (manager));
    }
  else
    {
      /* let next client interact */
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          EssmClient *cl = lp->data;
          if (essm_client_get_state (cl) == ESSM_CLIENT_WAITFORINTERACT)
            {
              essm_manager_start_interact (manager, cl);
              break;
            }
        }
    }

  /* restart save yourself timeout for client */
  essm_manager_start_client_save_timeout (manager, client);
}


static void
essm_manager_save_yourself_global (EssmManager     *manager,
                                   gint             save_type,
                                   gboolean         shutdown,
                                   gint             interact_style,
                                   gboolean         fast,
                                   EssmShutdownType shutdown_type,
                                   gboolean         allow_shutdown_save)
{
  gboolean  shutdown_save = allow_shutdown_save;
  GList    *lp;
  GError   *error = NULL;

  essm_verbose ("entering\n");

  if (shutdown)
    {
      if (!fast && shutdown_type == ESSM_SHUTDOWN_ASK)
        {
          /* if we're not specifying fast shutdown, and we're ok with
           * prompting then ask the user what to do */
          if (!essm_logout_dialog (manager->session_name,
                                   &manager->shutdown_type,
                                   &shutdown_save,
                                   manager->start_at))
            {
              return;
            }

          /* |allow_shutdown_save| is ignored if we prompt the user.  i think
           * this is the right thing to do. */
        }

      /* update shutdown type if we didn't prompt the user */
      if (shutdown_type != ESSM_SHUTDOWN_ASK)
        manager->shutdown_type = shutdown_type;

      essm_launch_desktop_files_on_shutdown (FALSE, manager->shutdown_type);

      /* we only save the session and quit if we're actually shutting down;
       * suspend, hibernate, hybrid sleep and switch user will (if successful) return us to
       * exactly the same state, so there's no need to save session */
      if (manager->shutdown_type == ESSM_SHUTDOWN_SUSPEND
          || manager->shutdown_type == ESSM_SHUTDOWN_HIBERNATE
          || manager->shutdown_type == ESSM_SHUTDOWN_HYBRID_SLEEP
          || manager->shutdown_type == ESSM_SHUTDOWN_SWITCH_USER)
        {
          if (!essm_shutdown_try_type (manager->shutdown_helper,
                                       manager->shutdown_type,
                                       &error))
            {
              essm_verbose ("failed calling shutdown, error message was %s", error->message);
              expidus_message_dialog (NULL, _("Shutdown Failed"),
                                   "dialog-error",
                                   manager->shutdown_type == ESSM_SHUTDOWN_SUSPEND
                                   ? _("Failed to suspend session")
                                   : manager->shutdown_type == ESSM_SHUTDOWN_HIBERNATE
                                   ? _("Failed to hibernate session")
                                   : manager->shutdown_type == ESSM_SHUTDOWN_HYBRID_SLEEP
                                   ? _("Failed to hybrid sleep session")
                                   : _("Failed to switch user"),
                                   error->message,
                                   EXPIDUS_BUTTON_TYPE_MIXED, "window-close-symbolic", _("_Close"), GTK_RESPONSE_ACCEPT,
                                   NULL);
              g_error_free (error);
            }

          /* at this point, either we failed to suspend/hibernate/hybrid sleep/switch user,
           * or we successfully did and we've been woken back
           * up or returned to the session, so return control to the user */
          return;
        }
    }

  /* don't save the session if shutting down without save */
  manager->save_session = !shutdown || shutdown_save;

  if (save_type == SmSaveBoth && !manager->save_session)
    {
      /* saving the session, so clients should
       * (prompt to) save the user data only */
      save_type = SmSaveGlobal;
    }

  essm_manager_set_state (manager,
                          shutdown
                          ? ESSM_MANAGER_SHUTDOWN
                          : ESSM_MANAGER_CHECKPOINT);

  /* handle legacy applications first! */
  if (manager->save_session)
      essm_legacy_perform_session_save ();

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = lp->data;
      EssmProperties *properties = essm_client_get_properties (client);
      const gchar *program;

      /* xterm's session management is broken, so we won't
       * send a SAVE YOURSELF to xterms */
      program = essm_properties_get_string (properties, SmProgram);
      if (program != NULL && strcasecmp (program, "xterm") == 0)
        continue;

      if (essm_client_get_state (client) != ESSM_CLIENT_SAVINGLOCAL)
        {
          SmsConn sms = essm_client_get_sms_connection (client);
          if (sms != NULL)
            {
              SmsSaveYourself (sms, save_type, shutdown, interact_style, fast);
            }
        }

      essm_client_set_state (client, ESSM_CLIENT_SAVING);
      essm_manager_start_client_save_timeout (manager, client);
    }
}


void
essm_manager_save_yourself (EssmManager *manager,
                            EssmClient  *client,
                            gint         save_type,
                            gboolean     shutdown,
                            gint         interact_style,
                            gboolean     fast,
                            gboolean     global)
{
  essm_verbose ("entering\n");

  if (G_UNLIKELY (essm_client_get_state (client) != ESSM_CLIENT_IDLE))
    {


      essm_verbose ("Client Id = %s, requested SAVE YOURSELF, but client is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
      return;
    }
  else if (G_UNLIKELY (manager->state != ESSM_MANAGER_IDLE))
    {
      essm_verbose ("Client Id = %s, requested SAVE YOURSELF, but manager is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
      return;
    }

  if (!global)
    {
      SmsConn sms = essm_client_get_sms_connection (client);
      /* client requests a local checkpoint. We slightly ignore
       * shutdown here, since it does not make sense for a local
       * checkpoint.
       */
      if (sms != NULL)
        {
          SmsSaveYourself (sms, save_type, FALSE, interact_style, fast);
        }
      essm_client_set_state (client, ESSM_CLIENT_SAVINGLOCAL);
      essm_manager_start_client_save_timeout (manager, client);
    }
  else
    essm_manager_save_yourself_global (manager, save_type, shutdown, interact_style, fast, ESSM_SHUTDOWN_ASK, TRUE);
}


void
essm_manager_save_yourself_phase2 (EssmManager *manager,
                                   EssmClient *client)
{
  essm_verbose ("entering\n");

  if (manager->state != ESSM_MANAGER_CHECKPOINT && manager->state != ESSM_MANAGER_SHUTDOWN)
    {
      SmsConn sms = essm_client_get_sms_connection (client);
      if (sms != NULL)
        {
          SmsSaveYourselfPhase2 (sms);
        }
      essm_client_set_state (client, ESSM_CLIENT_SAVINGLOCAL);
      essm_manager_start_client_save_timeout (manager, client);
    }
  else
    {
      essm_client_set_state (client, ESSM_CLIENT_WAITFORPHASE2);
      essm_manager_cancel_client_save_timeout (manager, client);

      if (!essm_manager_check_clients_saving (manager))
        essm_manager_maybe_enter_phase2 (manager);
    }
}


void
essm_manager_save_yourself_done (EssmManager *manager,
                                 EssmClient  *client,
                                 gboolean     success)
{
  essm_verbose ("entering\n");

  /* In essm_manager_interact_done we send SmsShutdownCancelled to clients in
     ESSM_CLIENT_WAITFORINTERACT state. They respond with SmcSaveYourselfDone
     (xsmp_shutdown_cancelled in libexpidus1ui library) so we allow it here. */
  if (essm_client_get_state (client) != ESSM_CLIENT_SAVING &&
      essm_client_get_state (client) != ESSM_CLIENT_SAVINGLOCAL &&
      essm_client_get_state (client) != ESSM_CLIENT_WAITFORINTERACT)
    {
      essm_verbose ("Client Id = %s send SAVE YOURSELF DONE, while not being "
                    "in save mode. Prepare to be nuked!\n",
                    essm_client_get_id (client));

      essm_manager_close_connection (manager, client, TRUE);
    }

  /* remove client save timeout, as client responded in time */
  essm_manager_cancel_client_save_timeout (manager, client);

  if (essm_client_get_state (client) == ESSM_CLIENT_SAVINGLOCAL)
    {
      SmsConn sms = essm_client_get_sms_connection (client);
      /* client completed local SaveYourself */
      essm_client_set_state (client, ESSM_CLIENT_IDLE);
      if (sms != NULL)
        {
          SmsSaveComplete (sms);
        }
    }
  else if (manager->state != ESSM_MANAGER_CHECKPOINT && manager->state != ESSM_MANAGER_SHUTDOWN)
    {
      essm_verbose ("Client Id = %s, send SAVE YOURSELF DONE, but manager is not in CheckPoint/Shutdown mode.\n"
                    "   Client will be nuked now.\n\n",
                    essm_client_get_id (client));
      essm_manager_close_connection (manager, client, TRUE);
    }
  else
    {
      essm_client_set_state (client, ESSM_CLIENT_SAVEDONE);
      essm_manager_complete_saveyourself (manager);
    }
}


void
essm_manager_close_connection (EssmManager *manager,
                               EssmClient  *client,
                               gboolean     cleanup)
{
  IceConn ice_conn;
  GList *lp;

  essm_client_set_state (client, ESSM_CLIENT_DISCONNECTED);
  essm_manager_cancel_client_save_timeout (manager, client);

  if (cleanup)
    {
      SmsConn sms_conn = essm_client_get_sms_connection (client);
      if (sms_conn != NULL)
        {
          ice_conn = SmsGetIceConnection (sms_conn);
          SmsCleanUp (sms_conn);
          IceSetShutdownNegotiation (ice_conn, False);
          IceCloseConnection (ice_conn);
        }
      else
        {
          essm_client_terminate (client);
        }
    }

  if (manager->state == ESSM_MANAGER_SHUTDOWNPHASE2)
    {
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          EssmClient *cl = lp->data;
          if (essm_client_get_state (cl) != ESSM_CLIENT_DISCONNECTED)
            return;
        }

      /* all clients finished the DIE phase in time */
      if (manager->die_timeout_id)
        {
          g_source_remove (manager->die_timeout_id);
          manager->die_timeout_id = 0;
        }
      g_signal_emit(G_OBJECT(manager), manager_signals[MANAGER_QUIT], 0);
    }
  else if (manager->state == ESSM_MANAGER_SHUTDOWN || manager->state == ESSM_MANAGER_CHECKPOINT)
    {
      essm_verbose ("Client Id = %s, closed connection in checkpoint state\n"
                    "   Session manager will show NO MERCY\n\n",
                    essm_client_get_id (client));

      /* stupid client disconnected in CheckPoint state, prepare to be nuked! */
      g_queue_remove (manager->running_clients, client);
      g_object_unref (client);
      essm_manager_complete_saveyourself (manager);
    }
  else
    {
      EssmProperties *properties = essm_client_steal_properties (client);

      if (properties != NULL)
        {
          if (essm_properties_check (properties))
            {
              if (essm_manager_handle_failed_properties (manager, properties) == FALSE)
                essm_properties_free (properties);
            }
          else
            essm_properties_free (properties);
        }

      /* regardless of the restart style hint, the current instance of
       * the client is gone, so remove it from the client list and free it. */
      g_queue_remove (manager->running_clients, client);
      g_object_unref (client);
    }
}


void
essm_manager_close_connection_by_ice_conn (EssmManager *manager,
                                           IceConn      ice_conn)
{
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = lp->data;
      SmsConn sms = essm_client_get_sms_connection (client);

      if (sms != NULL && SmsGetIceConnection (sms) == ice_conn)
        {
          essm_manager_close_connection (manager, client, FALSE);
          break;
        }
    }

  /* be sure to close the Ice connection in any case */
  IceSetShutdownNegotiation (ice_conn, False);
  IceCloseConnection (ice_conn);
}


gboolean
essm_manager_terminate_client (EssmManager *manager,
                               EssmClient  *client,
                               GError **error)
{
  SmsConn sms = essm_client_get_sms_connection (client);

  if (manager->state != ESSM_MANAGER_IDLE
      || essm_client_get_state (client) != ESSM_CLIENT_IDLE)
    {
      if (error)
        {
          g_set_error (error, ESSM_ERROR, ESSM_ERROR_BAD_STATE,
                       _("Can only terminate clients when in the idle state"));
        }
      return FALSE;
    }

  if (sms != NULL)
    {
      SmsDie (sms);
    }
  else
    {
      essm_client_terminate (client);
    }

  return TRUE;
}


static gboolean
manager_quit_signal (gpointer user_data)
{
  EssmManager *manager = user_data;

  g_signal_emit(G_OBJECT(manager), manager_signals[MANAGER_QUIT], 0);
  return FALSE;
}


void
essm_manager_perform_shutdown (EssmManager *manager)
{
  GList *lp;

  essm_verbose ("entering\n");

  /* send SmDie message to all clients */
  essm_manager_set_state (manager, ESSM_MANAGER_SHUTDOWNPHASE2);
  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = lp->data;
      SmsConn sms = essm_client_get_sms_connection (client);
      if (sms != NULL)
        {
          SmsDie (sms);
        }
      else
        {
          essm_client_end_session (client);
        }
    }

  /* check for SmRestartAnyway clients that have already quit and
   * set a ShutdownCommand */
  for (lp = g_queue_peek_nth_link (manager->restart_properties, 0);
       lp;
       lp = lp->next)
    {
      EssmProperties *properties = lp->data;
      gint            restart_style_hint;
      gchar         **shutdown_command;

      restart_style_hint = essm_properties_get_uchar (properties,
                                                      SmRestartStyleHint,
                                                      SmRestartIfRunning);
      shutdown_command = essm_properties_get_strv (properties, SmShutdownCommand);

      if (restart_style_hint == SmRestartAnyway && shutdown_command != NULL)
        {
          essm_verbose ("Client Id = %s, quit already, running shutdown command.\n\n",
                        properties->client_id);

          g_spawn_sync (essm_properties_get_string (properties, SmCurrentDirectory),
                        shutdown_command,
                        essm_properties_get_strv (properties, SmEnvironment),
                        G_SPAWN_SEARCH_PATH,
                        NULL, NULL,
                        NULL, NULL,
                        NULL, NULL);
        }
    }

  /* give all clients the chance to close the connection */
  manager->die_timeout_id = g_timeout_add (DIE_TIMEOUT,
                                           manager_quit_signal,
                                           manager);
}


gboolean
essm_manager_check_clients_saving (EssmManager *manager)
{
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = lp->data;
      EssmClientState state = essm_client_get_state (client);
      switch (state)
        {
          case ESSM_CLIENT_SAVING:
          case ESSM_CLIENT_WAITFORINTERACT:
          case ESSM_CLIENT_INTERACTING:
            return TRUE;
          default:
            break;
        }
    }

  return FALSE;
}


gboolean
essm_manager_maybe_enter_phase2 (EssmManager *manager)
{
  gboolean entered_phase2 = FALSE;
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = lp->data;

      if (essm_client_get_state (client) == ESSM_CLIENT_WAITFORPHASE2)
        {
          SmsConn sms = essm_client_get_sms_connection (client);
          entered_phase2 = TRUE;

          if (sms != NULL)
            {
              SmsSaveYourselfPhase2 (sms);
            }

          essm_client_set_state (client, ESSM_CLIENT_SAVING);
          essm_manager_start_client_save_timeout (manager, client);

          essm_verbose ("Client Id = %s enters SAVE YOURSELF PHASE2.\n\n",
                        essm_client_get_id (client));
        }
    }

  return entered_phase2;
}


void
essm_manager_complete_saveyourself (EssmManager *manager)
{
  GList *lp;

  /* Check if still clients in SAVING state or if we have to enter PHASE2
   * now. In either case, SaveYourself cannot be completed in this run.
   */
  if (essm_manager_check_clients_saving (manager) || essm_manager_maybe_enter_phase2 (manager))
    return;

  essm_verbose ("Manager finished SAVE YOURSELF, session data will be stored now.\n\n");

  /* all clients done, store session data */
  if (manager->save_session)
    essm_manager_store_session (manager);

  if (manager->state == ESSM_MANAGER_CHECKPOINT)
    {
      /* reset all clients to idle state */
      essm_manager_set_state (manager, ESSM_MANAGER_IDLE);
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          EssmClient *client = lp->data;
          SmsConn sms = essm_client_get_sms_connection (client);

          essm_client_set_state (client, ESSM_CLIENT_IDLE);
          if (sms != NULL)
            {
              SmsSaveComplete (sms);
            }
        }
    }
  else
    {
      /* shutdown the session */
      essm_manager_perform_shutdown (manager);
    }
}


static gboolean
essm_manager_save_timeout (gpointer user_data)
{
  EssmSaveTimeoutData *stdata = user_data;

  essm_verbose ("Client id = %s, received SAVE TIMEOUT\n"
                "   Client will be disconnected now.\n\n",
                essm_client_get_id (stdata->client));

  /* returning FALSE below will free the data */
  g_object_steal_data (G_OBJECT (stdata->client), "--save-timeout-id");

  essm_manager_close_connection (stdata->manager, stdata->client, TRUE);

  return FALSE;
}


static void
essm_manager_start_client_save_timeout (EssmManager *manager,
                                        EssmClient  *client)
{
  EssmSaveTimeoutData *sdata = g_new(EssmSaveTimeoutData, 1);

  sdata->manager = manager;
  sdata->client = client;
  /* |sdata| will get freed when the source gets removed */
  sdata->timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, SAVE_TIMEOUT,
                                          essm_manager_save_timeout,
                                          sdata, (GDestroyNotify) g_free);
  /* ... or, if the object gets destroyed first, the source will get
   * removed and will free |sdata| for us.  also, if there's a pending
   * timer, this call will clear it. */
  g_object_set_data_full (G_OBJECT (client), "--save-timeout-id",
                          GUINT_TO_POINTER (sdata->timeout_id),
                          (GDestroyNotify) G_CALLBACK (g_source_remove));
}


static void
essm_manager_cancel_client_save_timeout (EssmManager *manager,
                                         EssmClient  *client)
{
  /* clearing out the data will call g_source_remove(), which will free it */
  g_object_set_data (G_OBJECT (client), "--save-timeout-id", NULL);
}


void
essm_manager_store_session (EssmManager *manager)
{
  WnckWorkspace *workspace;
  GdkDisplay    *display;
  WnckScreen    *screen;
  ExpidusRc        *rc;
  GList         *lp;
  gchar          prefix[64];
  gchar         *backup;
  gchar         *group;
  gint           count = 0;
  gint           n, m;

  /* open file for writing, creates it if it doesn't exist */
  rc = expidus_rc_simple_open (manager->session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      fprintf (stderr,
               "expidus1-session: Unable to open session file %s for "
               "writing. Session data will not be stored. Please check "
               "your installation.\n",
               manager->session_file);
      return;
    }

  /* backup the old session file first */
  if (g_file_test (manager->session_file, G_FILE_TEST_IS_REGULAR))
    {
      backup = g_strconcat (manager->session_file, ".bak", NULL);
      unlink (backup);
      if (link (manager->session_file, backup))
          g_warning ("Failed to create session file backup");
      g_free (backup);
    }

  if (manager->state == ESSM_MANAGER_CHECKPOINT && manager->checkpoint_session_name != NULL)
    group = g_strconcat ("Session: ", manager->checkpoint_session_name, NULL);
  else
    group = g_strconcat ("Session: ", manager->session_name, NULL);
  expidus_rc_delete_group (rc, group, TRUE);
  expidus_rc_set_group (rc, group);
  g_free (group);

  for (lp = g_queue_peek_nth_link (manager->restart_properties, 0);
       lp;
       lp = lp->next)
    {
      EssmProperties *properties = lp->data;
      g_snprintf (prefix, 64, "Client%d_", count);
      essm_properties_store (properties, rc, prefix);
      ++count;
    }

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient     *client     = lp->data;
      EssmProperties *properties = essm_client_get_properties (client);
      gint            restart_style_hint;

      if (properties == NULL || !essm_properties_check (essm_client_get_properties (client)))
        continue;
      restart_style_hint = essm_properties_get_uchar (properties,
                                                      SmRestartStyleHint,
                                                      SmRestartIfRunning);
      if (restart_style_hint == SmRestartNever)
        continue;

      g_snprintf (prefix, 64, "Client%d_", count);
      essm_properties_store (essm_client_get_properties (client), rc, prefix);
      ++count;
    }

  expidus_rc_write_int_entry (rc, "Count", count);

  /* store legacy applications state */
  essm_legacy_store_session (rc);

  /* store current workspace numbers */
  display = gdk_display_get_default ();
  for (n = 0; n < XScreenCount (gdk_x11_display_get_xdisplay (display)); ++n)
    {
      screen = wnck_screen_get (n);
      wnck_screen_force_update (screen);

      workspace = wnck_screen_get_active_workspace (screen);
      m = wnck_workspace_get_number (workspace);

      g_snprintf (prefix, 64, "Screen%d_ActiveWorkspace", n);
      expidus_rc_write_int_entry (rc, prefix, m);
    }

  /* remember time */
  expidus_rc_write_int_entry (rc, "LastAccess", time (NULL));

  expidus_rc_close (rc);

  g_free (manager->checkpoint_session_name);
  manager->checkpoint_session_name = NULL;
}


EssmShutdownType
essm_manager_get_shutdown_type (EssmManager *manager)
{
  return manager->shutdown_type;
}


EssmManagerState
essm_manager_get_state (EssmManager *manager)
{
  return manager->state;
}


GQueue *
essm_manager_get_queue (EssmManager         *manager,
                        EssmManagerQueueType q_type)
{
  switch(q_type)
    {
      case ESSM_MANAGER_QUEUE_PENDING_PROPS:
        return manager->pending_properties;
      case ESSM_MANAGER_QUEUE_STARTING_PROPS:
        return manager->starting_properties;
      case ESSM_MANAGER_QUEUE_RESTART_PROPS:
        return manager->restart_properties;
      case ESSM_MANAGER_QUEUE_RUNNING_CLIENTS:
        return manager->running_clients;
      default:
        g_warning ("Requested invalid queue type %d", (gint)q_type);
        return NULL;
    }
}


gboolean
essm_manager_get_use_failsafe_mode (EssmManager *manager)
{
  return manager->failsafe_mode;
}


void
essm_manager_increase_failsafe_pending_clients (EssmManager *manager)
{
  manager->failsafe_clients_pending++;
}


gboolean
essm_manager_get_compat_startup (EssmManager          *manager,
                                 EssmManagerCompatType type)
{
  switch (type)
    {
      case ESSM_MANAGER_COMPAT_GNOME:
        return manager->compat_gnome;
      case ESSM_MANAGER_COMPAT_KDE:
        return manager->compat_kde;
      default:
        g_warning ("Invalid compat startup type %d", type);
        return FALSE;
    }
}


gboolean
essm_manager_get_start_at (EssmManager *manager)
{
  return manager->start_at;
}


/*
 * dbus server impl
 */

static gboolean essm_manager_dbus_get_info (EssmDbusManager *object,
                                            GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_list_clients (EssmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_get_state (EssmDbusManager *object,
                                             GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_checkpoint (EssmDbusManager *object,
                                              GDBusMethodInvocation *invocation,
                                              const gchar *arg_session_name);
static gboolean essm_manager_dbus_logout (EssmDbusManager *object,
                                          GDBusMethodInvocation *invocation,
                                          gboolean arg_show_dialog,
                                          gboolean arg_allow_save);
static gboolean essm_manager_dbus_shutdown (EssmDbusManager *object,
                                            GDBusMethodInvocation *invocation,
                                            gboolean arg_allow_save);
static gboolean essm_manager_dbus_can_shutdown (EssmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_restart (EssmDbusManager *object,
                                           GDBusMethodInvocation *invocation,
                                           gboolean arg_allow_save);
static gboolean essm_manager_dbus_can_restart (EssmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_suspend (EssmDbusManager *object,
                                           GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_can_suspend (EssmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_hibernate (EssmDbusManager *object,
                                             GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_can_hibernate (EssmDbusManager *object,
                                                 GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_hybrid_sleep (EssmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_can_hybrid_sleep (EssmDbusManager *object,
                                                    GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_switch_user (EssmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean essm_manager_dbus_register_client (EssmDbusManager *object,
                                                   GDBusMethodInvocation *invocation,
                                                   const gchar *arg_app_id,
                                                   const gchar *arg_client_startup_id);
static gboolean essm_manager_dbus_unregister_client (EssmDbusManager *object,
                                                     GDBusMethodInvocation *invocation,
                                                     const gchar *arg_client_id);


/* eader needs the above fwd decls */
#include <expidus1-session/essm-manager-dbus.h>


static void
remove_clients_for_connection (EssmManager *manager,
                               const gchar *service_name)
{
  GList       *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = ESSM_CLIENT (lp->data);
      if (g_strcmp0 (essm_client_get_service_name (client), service_name) == 0)
        {
          essm_manager_close_connection (manager, client, FALSE);
        }
    }
}

static void
on_name_owner_notify (GDBusConnection *connection,
                      const gchar     *sender_name,
                      const gchar     *object_path,
                      const gchar     *interface_name,
                      const gchar     *signal_name,
                      GVariant        *parameters,
                      gpointer         user_data)
{
        EssmManager *manager = ESSM_MANAGER (user_data);
        gchar       *service_name,
                    *old_service_name,
                    *new_service_name;

        g_variant_get (parameters, "(sss)", &service_name, &old_service_name, &new_service_name);

        if (strlen (new_service_name) == 0) {
                remove_clients_for_connection (manager, old_service_name);
        }
}

static void
essm_manager_dbus_class_init (EssmManagerClass *klass)
{
}


static void
essm_manager_dbus_init (EssmManager *manager, GDBusConnection *connection)
{
  GError *error = NULL;

  g_return_if_fail (ESSM_IS_MANAGER (manager));

  manager->connection = g_object_ref (connection);

  g_debug ("exporting path /com/expidus/SessionManager");

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (ESSM_DBUS_MANAGER (manager)),
                                         manager->connection,
                                         "/com/expidus/SessionManager",
                                         &error)) {
    if (error != NULL) {
            g_critical ("error exporting interface: %s", error->message);
            g_clear_error (&error);
            return;
    }
  }

  g_debug ("exported on %s", g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (ESSM_DBUS_MANAGER (manager))));

  manager->name_owner_id = g_dbus_connection_signal_subscribe (manager->connection,
                                                               "org.freedesktop.DBus",
                                                               "org.freedesktop.DBus",
                                                               "NameOwnerChanged",
                                                               "/org/freedesktop/DBus",
                                                               NULL,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               on_name_owner_notify,
                                                               manager,
                                                               NULL);
}


static void
essm_manager_iface_init (EssmDbusManagerIface *iface)
{
  iface->handle_can_hibernate = essm_manager_dbus_can_hibernate;
  iface->handle_can_hybrid_sleep = essm_manager_dbus_can_hybrid_sleep;
  iface->handle_can_restart = essm_manager_dbus_can_restart;
  iface->handle_can_shutdown = essm_manager_dbus_can_shutdown;
  iface->handle_can_suspend = essm_manager_dbus_can_suspend;
  iface->handle_checkpoint = essm_manager_dbus_checkpoint;
  iface->handle_get_info = essm_manager_dbus_get_info;
  iface->handle_get_state = essm_manager_dbus_get_state;
  iface->handle_hibernate = essm_manager_dbus_hibernate;
  iface->handle_hybrid_sleep = essm_manager_dbus_hybrid_sleep;
  iface->handle_switch_user = essm_manager_dbus_switch_user;
  iface->handle_list_clients = essm_manager_dbus_list_clients;
  iface->handle_logout = essm_manager_dbus_logout;
  iface->handle_restart = essm_manager_dbus_restart;
  iface->handle_shutdown = essm_manager_dbus_shutdown;
  iface->handle_suspend = essm_manager_dbus_suspend;
  iface->handle_register_client = essm_manager_dbus_register_client;
  iface->handle_unregister_client = essm_manager_dbus_unregister_client;
}

static void
essm_manager_dbus_cleanup (EssmManager *manager)
{
  if (manager->name_owner_id > 0 && manager->connection)
    {
      g_dbus_connection_signal_unsubscribe (manager->connection, manager->name_owner_id);
      manager->name_owner_id = 0;
    }

  if (G_LIKELY (manager->connection))
    {
      g_object_unref (manager->connection);
      manager->connection = NULL;
    }
}


static gboolean
essm_manager_dbus_get_info (EssmDbusManager *object,
                            GDBusMethodInvocation *invocation)
{
  essm_dbus_manager_complete_get_info (object, invocation, PACKAGE, VERSION, "Expidus");
  return TRUE;
}


static gboolean
essm_manager_dbus_list_clients (EssmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  EssmManager *manager = ESSM_MANAGER(object);
  GList  *lp;
  gint    i = 0;
  gint    num_clients;
  gchar **clients;

  num_clients = g_queue_get_length (manager->running_clients);
  clients = g_new0 (gchar*, num_clients + 1);
  clients[num_clients] = NULL;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = ESSM_CLIENT (lp->data);
      gchar *object_path = g_strdup (essm_client_get_object_path (client));
      clients[i] = object_path;
      i++;
    }

  essm_dbus_manager_complete_list_clients (object, invocation, (const gchar * const*)clients);
  g_strfreev (clients);
  return TRUE;
}


static gboolean
essm_manager_dbus_get_state (EssmDbusManager *object,
                             GDBusMethodInvocation *invocation)
{
  essm_dbus_manager_complete_get_state (object, invocation, ESSM_MANAGER(object)->state);
  return TRUE;
}


static gboolean
essm_manager_dbus_checkpoint_idled (gpointer data)
{
  EssmManager *manager = ESSM_MANAGER (data);

  essm_manager_save_yourself_global (manager, SmSaveBoth, FALSE,
                                     SmInteractStyleNone, FALSE,
                                     ESSM_SHUTDOWN_ASK, TRUE);

  return FALSE;
}


static gboolean
essm_manager_dbus_checkpoint (EssmDbusManager *object,
                              GDBusMethodInvocation *invocation,
                              const gchar *arg_session_name)
{
  EssmManager *manager = ESSM_MANAGER(object);

  if (manager->state != ESSM_MANAGER_IDLE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, _("Session manager must be in idle state when requesting a checkpoint"));
      return TRUE;
    }

  g_free (manager->checkpoint_session_name);
  if (arg_session_name[0] != '\0')
    manager->checkpoint_session_name = g_strdup (arg_session_name);
  else
    manager->checkpoint_session_name = NULL;

  /* idle so the dbus call returns in the client */
  g_idle_add (essm_manager_dbus_checkpoint_idled, manager);

  essm_dbus_manager_complete_checkpoint (object, invocation);
  return TRUE;
}


static gboolean
essm_manager_dbus_shutdown_idled (gpointer data)
{
  ShutdownIdleData *idata = data;

  essm_manager_save_yourself_global (idata->manager, SmSaveBoth, TRUE,
                                     SmInteractStyleAny, FALSE,
                                     idata->type, idata->allow_save);

  return FALSE;
}


static gboolean
essm_manager_save_yourself_dbus (EssmManager       *manager,
                                 EssmShutdownType   type,
                                 gboolean           allow_save)
{
  ShutdownIdleData *idata;

  if (manager->state != ESSM_MANAGER_IDLE)
    {
      return FALSE;
    }

  idata = g_new (ShutdownIdleData, 1);
  idata->manager = manager;
  idata->type = type;
  idata->allow_save = allow_save;
  g_idle_add_full (G_PRIORITY_DEFAULT, essm_manager_dbus_shutdown_idled,
                   idata, (GDestroyNotify) g_free);

  return TRUE;
}


static gboolean
essm_manager_dbus_logout (EssmDbusManager *object,
                          GDBusMethodInvocation *invocation,
                          gboolean arg_show_dialog,
                          gboolean arg_allow_save)
{
  EssmShutdownType type;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  type = arg_show_dialog ? ESSM_SHUTDOWN_ASK : ESSM_SHUTDOWN_LOGOUT;
  if (essm_manager_save_yourself_dbus (ESSM_MANAGER (object), type, arg_allow_save) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a shutdown"));
      return TRUE;
    }

  essm_dbus_manager_complete_logout (object, invocation);
  return TRUE;
}


static gboolean
essm_manager_dbus_shutdown (EssmDbusManager *object,
                            GDBusMethodInvocation *invocation,
                            gboolean arg_allow_save)
{
  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_manager_save_yourself_dbus (ESSM_MANAGER (object), ESSM_SHUTDOWN_SHUTDOWN, arg_allow_save) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a shutdown"));
      return TRUE;
    }

  essm_dbus_manager_complete_shutdown (object, invocation);
  return TRUE;
}


static gboolean
essm_manager_dbus_can_shutdown (EssmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  gboolean can_shutdown = FALSE;
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  essm_shutdown_can_shutdown (ESSM_MANAGER (object)->shutdown_helper, &can_shutdown, &error);

  if (error)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  essm_dbus_manager_complete_can_shutdown (object, invocation, can_shutdown);
  return TRUE;
}


static gboolean
essm_manager_dbus_restart (EssmDbusManager *object,
                           GDBusMethodInvocation *invocation,
                           gboolean arg_allow_save)
{
  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_manager_save_yourself_dbus (ESSM_MANAGER (object), ESSM_SHUTDOWN_RESTART, arg_allow_save) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a restart"));
      return TRUE;
    }

  essm_dbus_manager_complete_restart (object, invocation);
  return TRUE;
}


static gboolean
essm_manager_dbus_can_restart (EssmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  gboolean can_restart = FALSE;
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  essm_shutdown_can_restart (ESSM_MANAGER (object)->shutdown_helper, &can_restart, &error);

  if (error)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  essm_dbus_manager_complete_can_restart (object, invocation, can_restart);
  return TRUE;
}


static gboolean
essm_manager_dbus_suspend (EssmDbusManager *object,
                           GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_shutdown_try_suspend (ESSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  essm_dbus_manager_complete_suspend (object, invocation);
  return TRUE;
}


static gboolean
essm_manager_dbus_can_suspend (EssmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  gboolean auth_suspend = FALSE;
  gboolean can_suspend = FALSE;
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  essm_shutdown_can_suspend (ESSM_MANAGER (object)->shutdown_helper, &can_suspend, &auth_suspend, &error);

  if (error)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_suspend)
    can_suspend = FALSE;

  essm_dbus_manager_complete_can_suspend (object, invocation, can_suspend);
  return TRUE;
}

static gboolean
essm_manager_dbus_hibernate (EssmDbusManager *object,
                             GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_shutdown_try_hibernate (ESSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  essm_dbus_manager_complete_hibernate (object, invocation);
  return TRUE;
}

static gboolean
essm_manager_dbus_can_hibernate (EssmDbusManager *object,
                                 GDBusMethodInvocation *invocation)
{
  gboolean auth_hibernate = FALSE;
  gboolean can_hibernate = FALSE;
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  essm_shutdown_can_hibernate (ESSM_MANAGER (object)->shutdown_helper, &can_hibernate, &auth_hibernate, &error);

  if (error)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_hibernate)
    can_hibernate = FALSE;

  essm_dbus_manager_complete_can_hibernate (object, invocation, can_hibernate);
  return TRUE;
}

static gboolean
essm_manager_dbus_hybrid_sleep (EssmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_shutdown_try_hybrid_sleep (ESSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  essm_dbus_manager_complete_hybrid_sleep (object, invocation);
  return TRUE;
}

static gboolean
essm_manager_dbus_can_hybrid_sleep (EssmDbusManager *object,
                                    GDBusMethodInvocation *invocation)
{
  gboolean auth_hybrid_sleep = FALSE;
  gboolean can_hybrid_sleep = FALSE;
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);

  essm_shutdown_can_hybrid_sleep (ESSM_MANAGER (object)->shutdown_helper, &can_hybrid_sleep, &auth_hybrid_sleep, &error);

  if (error)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_hybrid_sleep)
    can_hybrid_sleep = FALSE;

  essm_dbus_manager_complete_can_hybrid_sleep (object, invocation, can_hybrid_sleep);
  return TRUE;
}

static gboolean
essm_manager_dbus_switch_user (EssmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  essm_verbose ("entering\n");

  g_return_val_if_fail (ESSM_IS_MANAGER (object), FALSE);
  if (essm_shutdown_try_switch_user (ESSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "%s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  essm_dbus_manager_complete_switch_user (object, invocation);
  return TRUE;
}



/* adapted from ConsoleKit2 whch was adapted from PolicyKit */
static gboolean
get_caller_info (EssmManager *manager,
                 const char  *sender,
                 pid_t       *calling_pid)
{
        gboolean  res   = FALSE;
        GVariant *value = NULL;
        GError   *error = NULL;

        if (sender == NULL) {
                essm_verbose ("sender == NULL");
                goto out;
        }

        if (manager->connection == NULL) {
                essm_verbose ("manager->connection == NULL");
                goto out;
        }

        value = g_dbus_connection_call_sync (manager->connection,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "GetConnectionUnixProcessID",
                                             g_variant_new ("(s)", sender),
                                             G_VARIANT_TYPE ("(u)"),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);

        if (value == NULL) {
                essm_verbose ("GetConnectionUnixProcessID() failed: %s", error->message);
                g_error_free (error);
                goto out;
        }
        g_variant_get (value, "(u)", calling_pid);
        g_variant_unref (value);

        res = TRUE;

out:
        return res;
}



static gboolean
essm_manager_dbus_register_client (EssmDbusManager *object,
                                   GDBusMethodInvocation *invocation,
                                   const gchar *arg_app_id,
                                   const gchar *arg_client_startup_id)
{
  EssmManager *manager;
  EssmClient  *client;
  gchar       *client_id;
  pid_t        pid = 0;

  manager = ESSM_MANAGER (object);

  if (arg_client_startup_id != NULL || (g_strcmp0 (arg_client_startup_id, "") == 0))
    {
      client_id = g_strdup_printf ("%s%s", arg_app_id, arg_client_startup_id);
    }
  else
    {
      client_id = g_strdup (arg_app_id);
    }

  /* create a new dbus-based client */
  client = essm_client_new (manager, NULL, manager->connection);

  /* register it so that it exports the dbus name */
  essm_manager_register_client (manager, client, client_id, NULL);

  /* save the app-id */
  essm_client_set_app_id (client, arg_app_id);

  /* attempt to get the caller'd pid */
  if (!get_caller_info (manager, g_dbus_method_invocation_get_sender (invocation), &pid))
    {
      pid = 0;
    }

  essm_client_set_pid (client, pid);

  /* we use the dbus service name to track clients so we know when they exit
   * or crash */
  essm_client_set_service_name (client, g_dbus_method_invocation_get_sender (invocation));

  essm_dbus_manager_complete_register_client (object, invocation, essm_client_get_object_path (client));
  g_free (client_id);
  return TRUE;
}



static gboolean
essm_manager_dbus_unregister_client (EssmDbusManager *object,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *arg_client_id)
{
  EssmManager *manager;
  GList       *lp;

  manager = ESSM_MANAGER (object);


  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      EssmClient *client = ESSM_CLIENT (lp->data);
      if (g_strcmp0 (essm_client_get_object_path (client), arg_client_id) == 0)
        {
          essm_manager_close_connection (manager, client, FALSE);
          essm_dbus_manager_complete_unregister_client (object, invocation);
          return TRUE;
        }
    }


  throw_error (invocation, ESSM_ERROR_BAD_VALUE, "Client with id of '%s' was not found", arg_client_id);
  return TRUE;
}
