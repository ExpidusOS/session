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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <libessm/essm-util.h>

#include <expidus1-session/essm-client.h>
#include <expidus1-session/essm-manager.h>
#include <expidus1-session/essm-global.h>
#include <expidus1-session/essm-marshal.h>
#include <expidus1-session/essm-error.h>
#include <expidus1-session/essm-client-dbus.h>

#define ESSM_CLIENT_OBJECT_PATH_PREFIX  "/org/expidus/SessionClients/"

struct _EssmClient
{
  EssmDbusClientSkeleton parent;

  EssmManager     *manager;

  gchar           *id;
  gchar           *app_id;
  gchar           *object_path;
  gchar           *service_name;
  guint            quit_timeout;

  EssmClientState  state;
  EssmProperties  *properties;
  SmsConn          sms_conn;
  GDBusConnection *connection;
};

typedef struct _EssmClientClass
{
  EssmDbusClientSkeletonClass parent;
} EssmClientClass;

typedef struct
{
  SmProp *props;
  gint    count;
} HtToPropsData;



static void essm_client_finalize (GObject *obj);

static void    essm_properties_discard_command_changed (EssmProperties *properties,
                                                        gchar         **old_discard);
static void    essm_client_dbus_class_init (EssmClientClass *klass);
static void    essm_client_dbus_init (EssmClient *client);
static void    essm_client_iface_init (EssmDbusClientIface *iface);
static void    essm_client_dbus_cleanup (EssmClient *client);


G_DEFINE_TYPE_WITH_CODE (EssmClient, essm_client, ESSM_DBUS_TYPE_CLIENT_SKELETON, G_IMPLEMENT_INTERFACE (ESSM_DBUS_TYPE_CLIENT, essm_client_iface_init));


static void
essm_client_class_init (EssmClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = essm_client_finalize;

  essm_client_dbus_class_init (klass);
}


static void
essm_client_init (EssmClient *client)
{

}

static void
essm_client_finalize (GObject *obj)
{
  EssmClient *client = ESSM_CLIENT (obj);

  essm_client_dbus_cleanup (client);

  if (client->properties != NULL)
    essm_properties_free (client->properties);

  if (client->quit_timeout != 0)
    g_source_remove (client->quit_timeout);

  g_free (client->id);
  g_free (client->app_id);
  g_free (client->object_path);
  g_free (client->service_name);

  G_OBJECT_CLASS (essm_client_parent_class)->finalize (obj);
}




static const gchar*
get_state (EssmClientState state)
{
  static const gchar *client_state[ESSM_CLIENT_STATE_COUNT] =
    {
      "ESSM_CLIENT_IDLE",
      "ESSM_CLIENT_INTERACTING",
      "ESSM_CLIENT_SAVEDONE",
      "ESSM_CLIENT_SAVING",
      "ESSM_CLIENT_SAVINGLOCAL",
      "ESSM_CLIENT_WAITFORINTERACT",
      "ESSM_CLIENT_WAITFORPHASE2",
      "ESSM_CLIENT_DISCONNECTED"
    };

  return client_state[state];
}

static void
essm_properties_discard_command_changed (EssmProperties *properties,
                                         gchar         **old_discard)
{
  gchar **new_discard;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (old_discard != NULL);

  new_discard = essm_properties_get_strv (properties, SmDiscardCommand);

  if (!essm_strv_equal (old_discard, new_discard))
    {
      essm_verbose ("Client Id = %s, running old discard command.\n\n",
                    properties->client_id);

      g_spawn_sync (essm_properties_get_string(properties, SmCurrentDirectory),
                    old_discard,
                    essm_properties_get_strv(properties, SmEnvironment),
                    G_SPAWN_SEARCH_PATH,
                    NULL, NULL,
                    NULL, NULL,
                    NULL, NULL);
    }
}


static void
essm_client_signal_prop_change (EssmClient *client,
                                const gchar *name)
{
  const GValue   *value;
  GVariant       *variant = NULL;
  EssmProperties *properties = client->properties;

  value = essm_properties_get (properties, name);
  if (value)
    {
      /* convert the gvalue to gvariant because gdbus requires it */
      if (G_VALUE_HOLDS_STRING (value))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING);
        }
      else if (G_VALUE_HOLDS_UCHAR (value))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE ("y"));
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING_ARRAY);
        }
      else
        {
          g_warning ("essm_client.c:essm_client_signal_prop_change: Value type not supported");
	  return;
        }

//      essm_dbus_client_emit_sm_property_changed (ESSM_DBUS_CLIENT (client), name, variant);
      g_variant_unref (variant);
    }
}



EssmClient*
essm_client_new (EssmManager *manager,
                 SmsConn      sms_conn,
                 GDBusConnection *connection)
{
  EssmClient *client;

  client = g_object_new (ESSM_TYPE_CLIENT, NULL);

  client->manager = manager;
  client->sms_conn = sms_conn;
  client->connection = g_object_ref (connection);
  client->state = ESSM_CLIENT_IDLE;

  return client;
}


void
essm_client_set_initial_properties (EssmClient     *client,
                                    EssmProperties *properties)
{
  g_return_if_fail (ESSM_IS_CLIENT (client));
  g_return_if_fail (properties != NULL);

  if (client->properties != NULL)
    essm_properties_free (client->properties);
  client->properties = properties;

  client->id = g_strdup (properties->client_id);

  g_free (client->object_path);
  client->object_path = g_strconcat (ESSM_CLIENT_OBJECT_PATH_PREFIX,
                                     client->id, NULL);
  g_strcanon (client->object_path + strlen (ESSM_CLIENT_OBJECT_PATH_PREFIX),
              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_",
              '_');

  essm_client_dbus_init (client);
}


EssmClientState
essm_client_get_state (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), ESSM_CLIENT_DISCONNECTED);
  return client->state;
}



static const gchar*
get_client_id (EssmClient *client)
{
  const gchar *client_id;

  if (client->app_id)
    client_id = client->app_id;
  else
    client_id = client->id;

  return client_id;
}



void
essm_client_set_state (EssmClient     *client,
                       EssmClientState state)
{
  g_return_if_fail (ESSM_IS_CLIENT (client));

  if (G_LIKELY (client->state != state))
    {
      EssmClientState old_state = client->state;
      client->state = state;
      essm_dbus_client_emit_state_changed (ESSM_DBUS_CLIENT (client), old_state, state);

      essm_verbose ("%s client state was %s and now is %s\n", get_client_id (client), get_state(old_state), get_state(state));

      /* During a save, we need to ask the client if it's ok to shutdown */
      if (state == ESSM_CLIENT_SAVING && essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWN)
        {
          essm_dbus_client_emit_query_end_session (ESSM_DBUS_CLIENT (client), 1);
        }
      else if (state == ESSM_CLIENT_SAVING && essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWNPHASE2)
        {
          essm_dbus_client_emit_end_session(ESSM_DBUS_CLIENT (client), 1);
        }
    }
}


const gchar *
essm_client_get_id (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), NULL);
  return client->id;
}


const gchar *
essm_client_get_app_id (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), NULL);
  return client->app_id;
}


SmsConn
essm_client_get_sms_connection (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), NULL);
  return client->sms_conn;
}


EssmProperties *
essm_client_get_properties (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), NULL);
  return client->properties;
}


EssmProperties *
essm_client_steal_properties (EssmClient *client)
{
  EssmProperties *properties;

  g_return_val_if_fail(ESSM_IS_CLIENT (client), NULL);

  properties = client->properties;
  client->properties = NULL;

  return properties;
}


void
essm_client_merge_properties (EssmClient *client,
                              SmProp    **props,
                              gint        num_props)
{
  EssmProperties *properties;
  SmProp         *prop;
  gint            n;

  g_return_if_fail (ESSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  for (n = 0; n < num_props; ++n)
    {
      gchar **old_discard = NULL;

      prop = props[n];

      if (!strcmp (prop->name, SmDiscardCommand))
        {
          old_discard = essm_properties_get_strv (properties, SmDiscardCommand);
          if (old_discard)
            old_discard = g_strdupv (old_discard);
        }

      if (essm_properties_set_from_smprop (properties, prop))
        {
          if (old_discard)
            essm_properties_discard_command_changed (properties, old_discard);

          essm_client_signal_prop_change (client, prop->name);
        }

      g_strfreev (old_discard);
    }
}


void
essm_client_delete_properties (EssmClient *client,
                               gchar     **prop_names,
                               gint        num_props)
{
  EssmProperties *properties;
  gint            n;

  g_return_if_fail (ESSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  for (n = 0; n < num_props; ++n)
    {
      if (essm_properties_remove (properties, prop_names[n]))
        {
          essm_dbus_client_emit_sm_property_deleted (ESSM_DBUS_CLIENT (client), prop_names[n]);
        }
    }
}


const gchar *
essm_client_get_object_path (EssmClient *client)
{
  g_return_val_if_fail (ESSM_IS_CLIENT (client), NULL);
  return client->object_path;
}




void
essm_client_set_service_name (EssmClient *client,
                              const gchar *service_name)
{
  g_free (client->service_name);
  client->service_name = g_strdup (service_name);
}


const gchar*
essm_client_get_service_name (EssmClient *client)
{
  return client->service_name;
}



static void
essm_client_save_restart_command (EssmClient *client)
{
  EssmProperties *properties = client->properties;
  gchar          *input;
  gchar          *output = NULL;
  gint            exit_status;
  GError         *error = NULL;

  input = g_strdup_printf ("ps -p %u -o args=", properties->pid);

  if(g_spawn_command_line_sync (input, &output, NULL, &exit_status, &error))
    {
      gchar **strv = g_new0(gchar*, 2);

      /* remove the newline at the end of the string */
      output[strcspn(output, "\n")] = 0;

      strv[0] = output;
      strv[1] = NULL;

      essm_verbose ("%s restart command %s\n", input, output);
      essm_properties_set_strv (properties, "RestartCommand", strv);
    }
  else
    {
      essm_verbose ("Failed to get the process command line using the command %s, error was %s\n", input, error->message);
    }

  g_free (input);
}



static void
essm_client_save_program_name (EssmClient *client)
{
  EssmProperties *properties = client->properties;
  gchar          *input;
  gchar          *output = NULL;
  gint            exit_status;
  GError         *error = NULL;

  input = g_strdup_printf ("ps -p %u -o comm=", properties->pid);

  if(g_spawn_command_line_sync (input, &output, NULL, &exit_status, &error))
    {
      /* remove the newline at the end of the string */
      output[strcspn(output, "\n")] = 0;

      essm_verbose ("%s program name %s\n", input, output);
      essm_properties_set_string (properties, "Program", output);
    }
  else
    {
      essm_verbose ("Failed to get the process command line using the command %s, error was %s\n", input, error->message);
    }

  g_free (input);
}



static void
essm_client_save_desktop_file (EssmClient *client)
{
  EssmProperties  *properties   = client->properties;
  GDesktopAppInfo *app_info     = NULL;
  const gchar     *app_id       = client->app_id;
  gchar           *desktop_file = NULL;

  if (app_id == NULL)
    return;

  /* First attempt to append .desktop to the filename since the desktop file
   * may match the application id. I.e. org.gnome.Devhelp.desktop matches
   * the GApplication org.gnome.Devhelp
   */
  desktop_file = g_strdup_printf("%s.desktop", app_id);
  essm_verbose ("looking for desktop file %s\n", desktop_file);
  app_info = g_desktop_app_info_new (desktop_file);

  if (app_info == NULL || g_desktop_app_info_get_filename (app_info) == NULL)
  {
    gchar *begin;
    g_free (desktop_file);
    desktop_file = NULL;

    /* Find the last '.' and try to load that. This is because the app_id is
     * in the funky org.expidus.parole format and the desktop file may just be
     * parole.desktop */
    begin = g_strrstr (app_id, ".");

    /* maybe it doesn't have dots in the name? */
    if (begin == NULL || begin++ == NULL)
      return;

    desktop_file = g_strdup_printf ("%s.desktop", begin);
    essm_verbose ("looking for desktop file %s\n", desktop_file);
    app_info = g_desktop_app_info_new (desktop_file);

    if (app_info == NULL || g_desktop_app_info_get_filename (app_info) == NULL)
      {
        /* Failed to get a desktop file, maybe it doesn't have one */
        essm_verbose ("failed to get a desktop file for the client\n");
        g_free (desktop_file);
        return;
      }
  }

  /* if we got here we found a .desktop file, save it */
  essm_properties_set_string (properties, GsmDesktopFile, g_desktop_app_info_get_filename (app_info));

  g_free (desktop_file);
}




void
essm_client_set_pid (EssmClient *client,
                     pid_t       pid)
{
  EssmProperties *properties;
  gchar          *pid_str;

  g_return_if_fail (ESSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  /* save the pid */
  properties->pid = pid;

  /* convert it to a string */
  pid_str = g_strdup_printf ("%d", pid);

  /* store the string as well (so we can export it over dbus */
  essm_properties_set_string (properties, "ProcessID", pid_str);

  /* save the command line for the process so we can restart it if needed */
  essm_client_save_restart_command (client);

  /* save the program name */
  essm_client_save_program_name (client);

  g_free (pid_str);
}


void
essm_client_set_app_id (EssmClient  *client,
                        const gchar *app_id)
{
  client->app_id = g_strdup (app_id);

  /* save the desktop file */
  essm_client_save_desktop_file (client);
}



static gboolean
kill_hung_client (gpointer user_data)
{
  EssmClient  *client = ESSM_CLIENT (user_data);

  client->quit_timeout = 0;

  if (!client->properties)
    return FALSE;

  if (client->properties->pid < 2)
    return FALSE;

  essm_verbose ("killing unresponsive client %s\n", get_client_id (client));
  kill (client->properties->pid, SIGKILL);

  return FALSE;
}



void
essm_client_terminate (EssmClient *client)
{
  essm_verbose ("emitting stop signal for client %s\n", get_client_id (client));

  /* Ask the client to shutdown gracefully */
  essm_dbus_client_emit_stop (ESSM_DBUS_CLIENT (client));

  /* add a timeout so we can forcefully stop the client */
  client->quit_timeout = g_timeout_add_seconds (15, kill_hung_client, client);
}



void
essm_client_end_session (EssmClient *client)
{
  essm_verbose ("emitting end session signal for client %s\n", get_client_id (client));

  /* Start the client shutdown */
  essm_dbus_client_emit_end_session (ESSM_DBUS_CLIENT (client), 1);
}


void essm_client_cancel_shutdown (EssmClient *client)
{
  essm_verbose ("emitting cancel session signal for client %s\n", get_client_id (client));

  /* Cancel the client shutdown */
  essm_dbus_client_emit_cancel_end_session (ESSM_DBUS_CLIENT (client));

}



/*
 * dbus server impl
 */

static gboolean essm_client_dbus_get_id (EssmDbusClient *object,
                                         GDBusMethodInvocation *invocation);
static gboolean essm_client_dbus_get_state (EssmDbusClient *object,
                                            GDBusMethodInvocation *invocation);
static gboolean essm_client_dbus_get_all_sm_properties (EssmDbusClient *object,
                                                        GDBusMethodInvocation *invocation);
static gboolean essm_client_dbus_get_sm_properties (EssmDbusClient *object,
                                                    GDBusMethodInvocation *invocation,
                                                    const gchar *const *arg_names);
static gboolean essm_client_dbus_set_sm_properties (EssmDbusClient *object,
                                                    GDBusMethodInvocation *invocation,
                                                    GVariant *arg_properties);
static gboolean essm_client_dbus_delete_sm_properties (EssmDbusClient *object,
                                                       GDBusMethodInvocation *invocation,
                                                       const gchar *const *arg_names);
static gboolean essm_client_dbus_terminate (EssmDbusClient *object,
                                            GDBusMethodInvocation *invocation);
static gboolean essm_client_dbus_end_session_response (EssmDbusClient *object,
                                                       GDBusMethodInvocation *invocation,
                                                       gboolean arg_is_ok,
                                                       const gchar *arg_reason);



static void
essm_client_dbus_class_init (EssmClientClass *klass)
{
}


static void
essm_client_dbus_init (EssmClient *client)
{
  GError *error = NULL;

  if (G_UNLIKELY(!client->connection))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      return;
    }

  essm_verbose ("exporting path %s\n", client->object_path);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (ESSM_DBUS_CLIENT (client)),
                                         client->connection,
                                         client->object_path,
                                         &error)) {
    if (error != NULL) {
            g_critical ("error exporting interface: %s", error->message);
            g_clear_error (&error);
            return;
    }
  }

  essm_verbose ("exported on %s\n", g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (ESSM_DBUS_CLIENT (client))));
}

static void
essm_client_iface_init (EssmDbusClientIface *iface)
{
        iface->handle_delete_sm_properties = essm_client_dbus_delete_sm_properties;
        iface->handle_get_all_sm_properties = essm_client_dbus_get_all_sm_properties;
        iface->handle_get_id = essm_client_dbus_get_id;
        iface->handle_get_sm_properties = essm_client_dbus_get_sm_properties;
        iface->handle_get_state = essm_client_dbus_get_state;
        iface->handle_set_sm_properties = essm_client_dbus_set_sm_properties;
        iface->handle_terminate = essm_client_dbus_terminate;
        iface->handle_end_session_response = essm_client_dbus_end_session_response;
}

static void
essm_client_dbus_cleanup (EssmClient *client)
{
  if (G_LIKELY (client->connection))
    {
      g_object_unref (client->connection);
      client->connection = NULL;
    }
}


static gboolean
essm_client_dbus_get_id (EssmDbusClient *object,
                         GDBusMethodInvocation *invocation)
{
  essm_dbus_client_complete_get_id (object, invocation, ESSM_CLIENT(object)->id);
  return TRUE;
}


static gboolean
essm_client_dbus_get_state (EssmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  essm_dbus_client_complete_get_state (object, invocation, ESSM_CLIENT(object)->state);
  return TRUE;
}


static void
builder_add_value (GVariantBuilder *builder,
                   const gchar  *name,
                   const GValue *value)
{
  if (name == NULL)
    {
      g_warning ("essm_client.c:builder_add_value: name must not be NULL");
      return;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING));
    }
  else if (G_VALUE_HOLDS_UCHAR (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE ("y")));
    }
  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING_ARRAY));
    }
  else
    {
      g_warning ("essm_client.c:builder_add_value: Value type not supported");
    }
}


static gboolean
essm_client_properties_tree_foreach (gpointer key,
                                     gpointer value,
                                     gpointer data)
{
  gchar  *prop_name = key;
  GValue *prop_value = value;
  GVariantBuilder *out_properties = data;

  builder_add_value (out_properties, prop_name, prop_value);
  return FALSE;
}

static gboolean
essm_client_dbus_get_all_sm_properties (EssmDbusClient *object,
                                        GDBusMethodInvocation *invocation)
{
  EssmProperties *properties = ESSM_CLIENT(object)->properties;
  GVariantBuilder out_properties;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, ESSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_builder_init (&out_properties, G_VARIANT_TYPE ("a{sv}"));

  g_tree_foreach (properties->sm_properties,
                  essm_client_properties_tree_foreach,
                  &out_properties);

  essm_dbus_client_complete_get_all_sm_properties (object, invocation, g_variant_builder_end (&out_properties));
  return TRUE;
}


static gboolean
essm_client_dbus_get_sm_properties (EssmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    const gchar *const *arg_names)
{
  EssmProperties *properties = ESSM_CLIENT(object)->properties;
  GVariantBuilder out_properties;
  gint            i;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, ESSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_builder_init (&out_properties, G_VARIANT_TYPE ("a{sv}"));

  for (i = 0; arg_names[i]; ++i)
    {
      GValue *value = g_tree_lookup (properties->sm_properties, arg_names[i]);

      if (value != NULL)
        builder_add_value (&out_properties, arg_names[i], value);
    }

  essm_dbus_client_complete_get_all_sm_properties (object, invocation, g_variant_builder_end (&out_properties));
  return TRUE;
}


static gboolean
essm_client_dbus_set_sm_properties (EssmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    GVariant *arg_properties)
{
  EssmProperties *properties = ESSM_CLIENT(object)->properties;
  GVariantIter   *iter;
  gchar          *prop_name;
  GVariant       *variant;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, ESSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_get (arg_properties, "a{sv}", &iter);

  while (g_variant_iter_next (iter, "{sv}", &prop_name, &variant))
    {
      GValue value;

      g_dbus_gvariant_to_gvalue (variant, &value);
      essm_properties_set (properties, prop_name, &value);

      g_variant_unref (variant);
    }

  essm_dbus_client_complete_set_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
essm_client_dbus_delete_sm_properties (EssmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *const *arg_names)
{
  EssmProperties *properties = ESSM_CLIENT(object)->properties;
  gchar **names = g_strdupv((gchar**)arg_names);

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, ESSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  essm_client_delete_properties (ESSM_CLIENT(object), names, g_strv_length (names));

  g_strfreev (names);
  essm_dbus_client_complete_delete_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
essm_client_dbus_terminate (EssmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  essm_manager_terminate_client (ESSM_CLIENT(object)->manager, ESSM_CLIENT(object), &error);
  if (error != NULL)
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE, "Unable to terminate client, error was: %s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  essm_dbus_client_complete_terminate (object, invocation);
  return TRUE;
}

static gboolean
essm_client_dbus_end_session_response (EssmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       gboolean arg_is_ok,
                                       const gchar *arg_reason)
{
  EssmClient *client = ESSM_CLIENT (object);

  essm_verbose ("got response for client %s, manager state is %s\n",
                get_client_id (client),
                essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWN ? "ESSM_MANAGER_SHUTDOWN" :
                essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWNPHASE2 ? "ESSM_MANAGER_SHUTDOWNPHASE2" :
                "Invalid time to respond");

  if (essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWN)
    {
      essm_manager_save_yourself_done (client->manager, client, arg_is_ok);
    }
  else if (essm_manager_get_state (client->manager) == ESSM_MANAGER_SHUTDOWNPHASE2)
    {
      essm_manager_close_connection (client->manager, client, TRUE);
    }
  else
    {
      throw_error (invocation, ESSM_ERROR_BAD_STATE,
                   "This method should be sent in response to a QueryEndSession or EndSession signal only");
      return TRUE;
    }

  essm_dbus_client_complete_end_session_response (object,  invocation);
  return TRUE;
}
