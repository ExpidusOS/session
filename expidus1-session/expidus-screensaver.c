/*
 * * Copyright (C) 2016 Eric Koegel <eric@expidus.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>

#include <libexpidus1util/libexpidus1util.h>
#include <esconf/esconf.h>

#include "expidus-screensaver.h"


#define HEARTBEAT_COMMAND       "heartbeat-command"
#define LOCK_COMMAND            "LockCommand"
#define XFPM_CHANNEL            "expidus1-power-manager"
#define XFPM_PROPERTIES_PREFIX  "/expidus1-power-manager/"
#define ESSM_CHANNEL            "expidus1-session"
#define ESSM_PROPERTIES_PREFIX  "/general/"

static void expidus_screensvaer_finalize   (GObject *object);

static void expidus_screensaver_set_property(GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void expidus_screensaver_get_property(GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec);


#define EXPIDUS_SCREENSAVER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), EXPIDUS_TYPE_SCREENSAVER, ExpidusScreenSaverPrivate))


typedef enum
{
    SCREENSAVER_TYPE_NONE,
    SCREENSAVER_TYPE_FREEDESKTOP,
    SCREENSAVER_TYPE_EXPIDUS,
    SCREENSAVER_TYPE_CINNAMON,
    SCREENSAVER_TYPE_MATE,
    SCREENSAVER_TYPE_GNOME,
    SCREENSAVER_TYPE_OTHER,
    N_SCREENSAVER_TYPE
} ScreenSaverType;

enum
{
    PROP_0 = 0,
    PROP_HEARTBEAT_COMMAND,
    PROP_LOCK_COMMAND
};

struct ExpidusScreenSaverPrivate
{
    guint            cookie;
    gchar           *heartbeat_command;
    gchar           *lock_command;
    GDBusProxy      *proxy;
    guint            screensaver_id;
    ScreenSaverType  screensaver_type;
    EsconfChannel   *xfpm_channel;
    EsconfChannel   *essm_channel;
};


G_DEFINE_TYPE (ExpidusScreenSaver, expidus_screensaver, G_TYPE_OBJECT)


static void
expidus_screensaver_class_init (ExpidusScreenSaverClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = expidus_screensvaer_finalize;
    object_class->set_property = expidus_screensaver_set_property;
    object_class->get_property = expidus_screensaver_get_property;

    g_type_class_add_private (klass, sizeof (ExpidusScreenSaverPrivate));

#define EXPIDUS_PARAM_FLAGS  (G_PARAM_READWRITE \
                         | G_PARAM_CONSTRUCT \
                         | G_PARAM_STATIC_NAME \
                         | G_PARAM_STATIC_NICK \
                         | G_PARAM_STATIC_BLURB)

    /* heartbeat command - to inhibit the screensaver from activating,
     * i.e. xscreensaver-command -deactivate */
    g_object_class_install_property(object_class, PROP_HEARTBEAT_COMMAND,
                                    g_param_spec_string(HEARTBEAT_COMMAND,
                                                        HEARTBEAT_COMMAND,
                                                        "Inhibit the screensaver from activating, "
                                                        "i.e. xscreensaver-command -deactivate",
                                                        "xscreensaver-command -deactivate",
                                                        EXPIDUS_PARAM_FLAGS));

    /* lock command - to lock the desktop, i.e. xscreensaver-command -lock */
    g_object_class_install_property(object_class, PROP_LOCK_COMMAND,
                                    g_param_spec_string(LOCK_COMMAND,
                                                        LOCK_COMMAND,
                                                        "Lock the desktop, i.e. "
                                                        "xscreensaver-command -lock",
                                                        "xscreensaver-command -lock",
                                                        EXPIDUS_PARAM_FLAGS));
#undef EXPIDUS_PARAM_FLAGS
}

static void
expidus_screensaver_set_property(GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    ExpidusScreenSaver *saver = EXPIDUS_SCREENSAVER (object);

    switch(property_id) {
        case PROP_HEARTBEAT_COMMAND:
        {
            g_free (saver->priv->heartbeat_command);
            saver->priv->heartbeat_command = g_value_dup_string (value);
            DBG ("saver->priv->heartbeat_command %s", saver->priv->heartbeat_command);
            break;
        }
        case PROP_LOCK_COMMAND:
        {
            g_free (saver->priv->lock_command);
            saver->priv->lock_command = g_value_dup_string (value);
            DBG ("saver->priv->lock_command %s", saver->priv->lock_command);
            break;
        }
        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
}

static void
expidus_screensaver_get_property(GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    ExpidusScreenSaver *saver = EXPIDUS_SCREENSAVER (object);

    switch(property_id) {
        case PROP_HEARTBEAT_COMMAND:
            g_value_set_string (value, saver->priv->heartbeat_command);
            break;

        case PROP_LOCK_COMMAND:
            g_value_set_string (value, saver->priv->lock_command);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static gboolean
screen_saver_proxy_setup(ExpidusScreenSaver *saver,
                         const gchar     *name,
                         const gchar     *object_path,
                         const gchar     *interface)
{
    GDBusProxy *proxy;

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           NULL,
                                           name,
                                           object_path,
                                           interface,
                                           NULL,
                                           NULL);

    if (proxy != NULL)
    {
        gchar *owner = NULL;
        /* is there anyone actually providing a service? */
        owner = g_dbus_proxy_get_name_owner (proxy);
        if (owner != NULL)
        {
            DBG ("proxy owner: %s", owner);
            saver->priv->proxy = proxy;
            g_free (owner);
            return TRUE;
        }
        else
        {
            /* not using this proxy, nobody's home */
            g_object_unref (proxy);
        }
    }

    return FALSE;
}

static void
expidus_screensaver_setup(ExpidusScreenSaver *saver)
{
    /* Try to use the freedesktop dbus API */
    if (screen_saver_proxy_setup (saver,
                                  "org.freedesktop.ScreenSaver",
                                  "/org/freedesktop/ScreenSaver",
                                  "org.freedesktop.ScreenSaver"))
    {
        DBG ("using freedesktop compliant screensaver daemon");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_FREEDESKTOP;
    } else if (screen_saver_proxy_setup (saver,
                                         "com.expidus.ScreenSaver",
                                         "/com/expidus/ScreenSaver",
                                         "com.expidus.ScreenSaver"))
    {
        DBG ("using expidus screensaver daemon");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_EXPIDUS;
    } else if (screen_saver_proxy_setup (saver,
                                         "org.cinnamon.ScreenSaver",
                                         "/org/cinnamon/ScreenSaver",
                                         "org.cinnamon.ScreenSaver"))
    {
        DBG ("using cinnamon screensaver daemon");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_CINNAMON;
    } else if (screen_saver_proxy_setup (saver,
                                         "org.mate.ScreenSaver",
                                         "/org/mate/ScreenSaver",
                                         "org.mate.ScreenSaver"))
    {
        DBG ("using mate screensaver daemon");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_MATE;
    } else if (screen_saver_proxy_setup (saver,
                                         "org.gnome.ScreenSaver",
                                         "/org/gnome/ScreenSaver",
                                         "org.gnome.ScreenSaver"))
    {
        DBG ("using gnome screensaver daemon");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_GNOME;
    }
    else
    {
        DBG ("using command line screensaver interface");
        saver->priv->screensaver_type = SCREENSAVER_TYPE_OTHER;
    }
}

static void
expidus_screensaver_init (ExpidusScreenSaver *saver)
{
    GError *error = NULL;

    saver->priv = EXPIDUS_SCREENSAVER_GET_PRIVATE (saver);

    if ( !esconf_init (&error) )
    {
        g_critical ("esconf_init failed: %s\n", error->message);
        g_clear_error (&error);
    }
    else
    {
            saver->priv->xfpm_channel = esconf_channel_get (XFPM_CHANNEL);
            saver->priv->essm_channel = esconf_channel_get (ESSM_CHANNEL);

    }

    expidus_screensaver_setup (saver);
}

static void
expidus_screensvaer_finalize (GObject *object)
{
    ExpidusScreenSaver *saver = EXPIDUS_SCREENSAVER (object);

    if (saver->priv->screensaver_id != 0)
    {
        g_source_remove (saver->priv->screensaver_id);
        saver->priv->screensaver_id = 0;
    }

    if (saver->priv->proxy)
    {
        g_object_unref (saver->priv->proxy);
        saver->priv->proxy = NULL;
    }

    if (saver->priv->heartbeat_command)
    {
        g_free (saver->priv->heartbeat_command);
        saver->priv->heartbeat_command = NULL;
    }

    if (saver->priv->lock_command)
    {
        g_free (saver->priv->heartbeat_command);
        saver->priv->heartbeat_command = NULL;
    }
}

/**
 * expidus_screensaver_new:
 *
 * Creates a new ExpidusScreenSaver object or increases the refrence count
 * of the current object. Call g_object_unref when finished.
 *
 * RETURNS: an ExpidusScreenSaver object
 **/
ExpidusScreenSaver *
expidus_screensaver_new (void)
{
    static gpointer *saver = NULL;

    if (saver != NULL)
    {
        g_object_ref (saver);
    }
    else
    {
        saver = g_object_new (EXPIDUS_TYPE_SCREENSAVER, NULL);

        g_object_add_weak_pointer (G_OBJECT (saver), (gpointer *) &saver);

        esconf_g_property_bind (EXPIDUS_SCREENSAVER (saver)->priv->xfpm_channel,
                                XFPM_PROPERTIES_PREFIX HEARTBEAT_COMMAND,
                                G_TYPE_STRING,
                                G_OBJECT(saver),
                                HEARTBEAT_COMMAND);

        esconf_g_property_bind (EXPIDUS_SCREENSAVER (saver)->priv->essm_channel,
                                ESSM_PROPERTIES_PREFIX LOCK_COMMAND,
                                G_TYPE_STRING,
                                G_OBJECT(saver),
                                LOCK_COMMAND);
    }

    return EXPIDUS_SCREENSAVER (saver);
}

static gboolean
expidus_reset_screen_saver (gpointer user_data)
{
    ExpidusScreenSaver *saver = user_data;

    TRACE("entering\n");

    /* If we found an interface during the setup, use it */
    if (saver->priv->proxy)
    {
        GVariant *response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                                     "SimulateUserActivity",
                                                     NULL,
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     NULL);
        if (response != NULL)
        {
            g_variant_unref (response);
        }
    } else if (saver->priv->heartbeat_command)
    {
        DBG ("running heartbeat command: %s", saver->priv->heartbeat_command);
        g_spawn_command_line_async (saver->priv->heartbeat_command, NULL);
    }

    /* continue until we're removed */
    return TRUE;
}

/**
 * expidus_screensaver_inhibit:
 * @saver: The ExpidusScreenSaver object
 * @inhibit: Wether to inhibit the screensaver from activating.
 *
 * Calling this function with inhibit as TRUE will prevent the user's
 * screensaver from activating. This is useful when the user is watching
 * a movie or giving a presentation.
 *
 * Calling this function with inhibit as FALSE will remove any current
 * screensaver inhibit the ExpidusScreenSaver object has.
 *
 **/
void
expidus_screensaver_inhibit (ExpidusScreenSaver *saver,
                          gboolean inhibit)
{
    if (saver->priv->screensaver_type != SCREENSAVER_TYPE_FREEDESKTOP &&
        saver->priv->screensaver_type != SCREENSAVER_TYPE_EXPIDUS &&
        saver->priv->screensaver_type != SCREENSAVER_TYPE_MATE)
    {
        /* remove any existing keepalive */
        if (saver->priv->screensaver_id != 0)
        {
            g_source_remove (saver->priv->screensaver_id);
            saver->priv->screensaver_id = 0;
        }

        if (inhibit)
        {
            /* Reset the screensaver timers every so often so they don't activate */
            saver->priv->screensaver_id = g_timeout_add_seconds (20,
                                                                 expidus_reset_screen_saver,
                                                                 saver);
        }
        return;
    }

    /* SCREENSAVER_TYPE_FREEDESKTOP & SCREENSAVER_TYPE_MATE
     * don't need a periodic timer because they have an actual
     * inhibit/uninhibit setup */
    if (inhibit)
    {
        GVariant *response = NULL;
        response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                           "Inhibit",
                                           g_variant_new ("(ss)",
                                                          PACKAGE_NAME,
                                                          "Inhibit requested"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           NULL);
        if (response != NULL)
        {
            g_variant_get (response, "(u)", &saver->priv->cookie);
            g_variant_unref (response);
        }
    }
    else
    {
        GVariant *response = NULL;
        response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                           "UnInhibit",
                                           g_variant_new ("(u)",
                                                          saver->priv->cookie),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           NULL);

        saver->priv->cookie = 0;
        if (response != NULL)
        {
            g_variant_unref (response);
        }
    }
}

/**
 * expidus_screensaver_lock:
 * @saver: The ExpidusScreenSaver object
 *
 * Attempts to lock the screen, either with one of the screensaver
 * dbus proxies, the esconf lock command, or one of the
 * fallback scripts such as xdg-screensaver.
 *
 * RETURNS TRUE if the lock attempt returns success.
 **/
gboolean
expidus_screensaver_lock (ExpidusScreenSaver *saver)
{
    switch (saver->priv->screensaver_type) {
        case SCREENSAVER_TYPE_FREEDESKTOP:
        case SCREENSAVER_TYPE_EXPIDUS:
        case SCREENSAVER_TYPE_MATE:
        case SCREENSAVER_TYPE_GNOME:
        {
            GVariant *response = NULL;
            response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                               "Lock",
                                               g_variant_new ("()"),
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               NULL);
            if (response != NULL)
            {
                g_variant_unref (response);
                return TRUE;
            }
            else
            {
                return FALSE;
            }
            break;
        }
        case SCREENSAVER_TYPE_CINNAMON:
        {
            GVariant *response = NULL;
            response = g_dbus_proxy_call_sync (saver->priv->proxy,
                                               "Lock",
                                               g_variant_new ("(s)", PACKAGE_NAME),
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               NULL);
            if (response != NULL)
            {
                g_variant_unref (response);
                return TRUE;
            }
            else
            {
                return FALSE;
            }
            break;
        }
        case SCREENSAVER_TYPE_OTHER:
        {
            gboolean ret = FALSE;

            if (saver->priv->lock_command != NULL)
            {
                DBG ("running lock command: %s", saver->priv->lock_command);
                ret = g_spawn_command_line_async (saver->priv->lock_command, NULL);
            }

            if (!ret)
            {
                g_warning ("Screensaver lock command not set when attempting to lock the screen.\n"
                           "Please set the esconf property %s%s in expidus1-session to the desired lock command",
                           ESSM_PROPERTIES_PREFIX, LOCK_COMMAND);

                ret = g_spawn_command_line_async ("eslock1", NULL);
            }

            if (!ret)
            {
                ret = g_spawn_command_line_async ("xdg-screensaver lock", NULL);
            }

            if (!ret)
            {
                ret = g_spawn_command_line_async ("xscreensaver-command -lock", NULL);
            }

            return ret;
            /* obviously we don't need this break statement but I'm sure some
             * compiler or static analysis tool will complain */
            break;
        }
        default:
        {
            g_warning ("Unknown screensaver type set when calling expidus_screensaver_lock");
            break;
        }
    }

    return FALSE;
}
