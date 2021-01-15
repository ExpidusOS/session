/*-
 * Copyright (c) 2010 Ali Abdallah <aliov@expidus.org>
 * Copyright (c) 2011 Nick Schermer <nick@expidus.org>
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


#include <gio/gio.h>

#include <expidus1-session/essm-consolekit.h>
#include <expidus1-session/expidus-screensaver.h>
#include <libessm/essm-util.h>
#include "essm-global.h"

#define CK_NAME         "org.freedesktop.ConsoleKit"
#define CK_MANAGER_PATH "/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_NAME CK_NAME ".Manager"



static void     essm_consolekit_finalize     (GObject         *object);
static void     essm_consolekit_proxy_free   (EssmConsolekit *consolekit);



struct _EssmConsolekitClass
{
  GObjectClass __parent__;
};

struct _EssmConsolekit
{
  GObject __parent__;

  GDBusProxy *proxy;
  guint name_id;
  ExpidusScreenSaver *screensaver;
};



G_DEFINE_TYPE (EssmConsolekit, essm_consolekit, G_TYPE_OBJECT)



static void
essm_consolekit_class_init (EssmConsolekitClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = essm_consolekit_finalize;
}



static void
name_acquired (GDBusConnection *connection,
               const gchar *name,
               const gchar *name_owner,
               gpointer user_data)
{
  EssmConsolekit *consolekit = user_data;

  essm_verbose ("%s started up, owned by %s\n", name, name_owner);

  if (consolekit->proxy != NULL)
  {
    essm_verbose ("already have a connection to consolekit\n");
    return;
  }

  consolekit->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                     NULL,
                                                     CK_NAME,
                                                     CK_MANAGER_PATH,
                                                     CK_MANAGER_NAME,
                                                     NULL,
                                                     NULL);
}



static void
name_lost (GDBusConnection *connection,
           const gchar *name,
           gpointer user_data)
{
  EssmConsolekit *consolekit = user_data;

  essm_verbose ("ck lost\n");

  essm_consolekit_proxy_free (consolekit);
}



static void
essm_consolekit_init (EssmConsolekit *consolekit)
{
  consolekit->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     CK_NAME,
                                                     CK_MANAGER_PATH,
                                                     CK_MANAGER_NAME,
                                                     NULL,
                                                     NULL);

  consolekit->name_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                          CK_NAME,
                                          G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                          name_acquired,
                                          name_lost,
                                          consolekit,
                                          NULL);

  consolekit->screensaver = expidus_screensaver_new ();
}



static void
essm_consolekit_finalize (GObject *object)
{
  essm_consolekit_proxy_free (ESSM_CONSOLEKIT (object));

  (*G_OBJECT_CLASS (essm_consolekit_parent_class)->finalize) (object);
}



static void
essm_consolekit_proxy_free (EssmConsolekit *consolekit)
{
  if (consolekit->proxy != NULL)
    {
      g_object_unref (G_OBJECT (consolekit->proxy));
      consolekit->proxy = NULL;
    }

  if (consolekit->screensaver != NULL)
    {
      g_object_unref (G_OBJECT (consolekit->screensaver));
      consolekit->screensaver = NULL;
    }
}



static gboolean
essm_consolekit_can_method (EssmConsolekit  *consolekit,
                            const gchar     *method,
                            gboolean        *can_method,
                            GError         **error)
{
  GVariant *variant = NULL;

  g_return_val_if_fail (can_method != NULL, FALSE);

  /* never return true if something fails */
  *can_method = FALSE;

  if (!consolekit->proxy)
  {
    essm_verbose ("no ck proxy\n");
    return FALSE;
  }

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    g_variant_new ("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_get_child (variant, 0, "b", can_method);

  g_variant_unref (variant);
  return TRUE;
}



static gboolean
essm_consolekit_can_sleep (EssmConsolekit  *consolekit,
                           const gchar     *method,
                           gboolean        *can_method,
                           gboolean        *auth_method,
                           GError         **error)
{
  gchar *can_string;
  GVariant *variant = NULL;

  g_return_val_if_fail (can_method != NULL, FALSE);

  /* never return true if something fails */
  *can_method = FALSE;
  *auth_method = FALSE;

  if (!consolekit->proxy)
  {
    essm_verbose ("no ck proxy\n");
    return FALSE;
  }

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    g_variant_new ("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_get_child (variant, 0, "s", &can_string);

  /* If yes or challenge then we can sleep, it just might take a password */
  if (g_strcmp0 (can_string, "yes") == 0 || g_strcmp0 (can_string, "challenge") == 0)
    {
      *can_method = TRUE;
      *auth_method = TRUE;
    }
  else
    {
      *can_method = FALSE;
      *auth_method = FALSE;
    }

  g_variant_unref (variant);
  return TRUE;
}



static gboolean
essm_consolekit_try_method (EssmConsolekit  *consolekit,
                            const gchar     *method,
                            GError         **error)
{
  GVariant *variant = NULL;

  if (!consolekit->proxy)
  {
    essm_verbose ("no ck proxy\n");
    return FALSE;
  }

  essm_verbose ("calling %s\n", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    g_variant_new ("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_unref (variant);
  return TRUE;
}



static gboolean
essm_consolekit_try_sleep (EssmConsolekit  *consolekit,
                           const gchar     *method,
                           GError         **error)
{
  GVariant *variant = NULL;

  if (!consolekit->proxy)
  {
    essm_verbose ("no ck proxy\n");
    return FALSE;
  }

  essm_verbose ("calling %s\n", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    g_variant_new_boolean (TRUE),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_unref (variant);
  return TRUE;
}



EssmConsolekit *
essm_consolekit_get (void)
{
  static EssmConsolekit *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (ESSM_TYPE_CONSOLEKIT, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
essm_consolekit_try_restart (EssmConsolekit  *consolekit,
                             GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_try_method (consolekit, "Restart", error);
}



gboolean
essm_consolekit_try_shutdown (EssmConsolekit  *consolekit,
                              GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_try_method (consolekit, "Stop", error);
}



gboolean
essm_consolekit_can_restart (EssmConsolekit  *consolekit,
                             gboolean        *can_restart,
                             GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);


  return essm_consolekit_can_method (consolekit, "CanRestart",
                                     can_restart, error);
}



gboolean
essm_consolekit_can_shutdown (EssmConsolekit  *consolekit,
                              gboolean        *can_shutdown,
                              GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_can_method (consolekit, "CanStop",
                                     can_shutdown, error);
}


static gboolean
lock_screen (EssmConsolekit  *consolekit,
             GError **error)
{
  EsconfChannel *channel;
  gboolean       ret = TRUE;

  channel = essm_open_config ();
  if (esconf_channel_get_bool (channel, "/shutdown/LockScreen", FALSE))
      ret = expidus_screensaver_lock (consolekit->screensaver);

  return ret;
}

gboolean
essm_consolekit_try_suspend (EssmConsolekit  *consolekit,
                             GError         **error)
{
  gboolean can_suspend, auth_suspend;

  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  /* Check if consolekit can suspend before we call lock screen. */
  if (essm_consolekit_can_suspend (consolekit, &can_suspend, &auth_suspend, NULL))
    {
      if (!can_suspend)
        return FALSE;
    }
  else
    {
      return FALSE;
    }

  if (!lock_screen (consolekit, error))
    return FALSE;

  return essm_consolekit_try_sleep (consolekit, "Suspend", error);
}



gboolean
essm_consolekit_try_hibernate (EssmConsolekit  *consolekit,
                               GError         **error)
{
  gboolean can_hibernate, auth_hibernate;

  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  /* Check if consolekit can hibernate before we call lock screen. */
  if (essm_consolekit_can_hibernate (consolekit, &can_hibernate, &auth_hibernate, NULL))
    {
      if (!can_hibernate)
        return FALSE;
    }
  else
    {
      return FALSE;
    }

  if (!lock_screen (consolekit, error))
    return FALSE;

  return essm_consolekit_try_sleep (consolekit, "Hibernate", error);
}



gboolean
essm_consolekit_try_hybrid_sleep (EssmConsolekit  *consolekit,
                                  GError         **error)
{
  gboolean can_hybrid_sleep, auth_hybrid_sleep;

  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  /* Check if consolekit can hybrid sleep before we call lock screen. */
  if (essm_consolekit_can_hybrid_sleep (consolekit, &can_hybrid_sleep, &auth_hybrid_sleep, NULL))
    {
      if (!can_hybrid_sleep)
        return FALSE;
    }
  else
    {
      return FALSE;
    }

  if (!lock_screen (consolekit, error))
    return FALSE;

  return essm_consolekit_try_sleep (consolekit, "HybridSleep", error);
}



gboolean
essm_consolekit_can_suspend (EssmConsolekit  *consolekit,
                             gboolean        *can_suspend,
                             gboolean        *auth_suspend,
                             GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_can_sleep (consolekit, "CanSuspend",
                                    can_suspend, auth_suspend, error);
}



gboolean
essm_consolekit_can_hibernate (EssmConsolekit  *consolekit,
                               gboolean        *can_hibernate,
                               gboolean        *auth_hibernate,
                               GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_can_sleep (consolekit, "CanHibernate",
                                    can_hibernate, auth_hibernate, error);
}



gboolean
essm_consolekit_can_hybrid_sleep (EssmConsolekit  *consolekit,
                                  gboolean        *can_hybrid_sleep,
                                  gboolean        *auth_hybrid_sleep,
                                  GError         **error)
{
  g_return_val_if_fail (ESSM_IS_CONSOLEKIT (consolekit), FALSE);

  return essm_consolekit_can_sleep (consolekit, "CanHybridSleep",
                                    can_hybrid_sleep, auth_hybrid_sleep, error);
}
