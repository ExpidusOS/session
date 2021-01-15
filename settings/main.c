/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <esconf/esconf.h>

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include <libessm/essm-util.h>

#include "esae-window.h"
#include "expidus1-session-settings-common.h"
#include "expidus1-session-settings_ui.h"


static void expidus1_session_settings_dialog_response (GtkDialog *dialog, gint response, gpointer userdata)
{
    if (response == GTK_RESPONSE_HELP) {
       expidus_dialog_show_help (GTK_WINDOW (dialog), "expidus1-session", "preferences", NULL);
    }
    else {
      gtk_widget_destroy(GTK_WIDGET(dialog));
      gtk_main_quit ();
    }
}

static void
expidus1_session_settings_show_saved_sessions (GtkBuilder *builder,
                                            ExpidusRc     *rc,
                                            gboolean    visible)
{
    GtkWidget *notebook = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
    GtkWidget *sessions_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saved-sessions-list"));
    GtkTreeModel    *model;
    GList *sessions;

    gtk_widget_set_visible (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), visible);
    if (visible == FALSE)
        return;

    settings_list_sessions_treeview_init (GTK_TREE_VIEW (sessions_treeview));
    sessions = settings_list_sessions (rc);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (sessions_treeview));
    settings_list_sessions_populate (model, sessions);
}

int
main(int argc,
     char **argv)
{
    GtkBuilder *builder;
    GtkWidget *notebook;
    GtkWidget *esae_page;
    GtkWidget *lbl;
    GtkWidget *label_active_session;
    GObject *delete_button;
    GObject *treeview;
    GError *error = NULL;
    EsconfChannel *channel;
    ExpidusRc *rc;
    gboolean visible;
    gchar *active_session;
    gchar *active_session_label;
    const gchar *format;
    gchar *markup;

    Window opt_socket_id = 0;
    gboolean opt_version = FALSE;

    GOptionEntry option_entries[] =
    {
        { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
        { NULL }
    };

    expidus_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args (&argc, &argv, NULL, option_entries,
                            GETTEXT_PACKAGE, &error))
    {
        if(G_LIKELY(error)) {
            g_print("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print("\n");
            g_error_free (error);
        } else
            g_error("Unable to open display.");

        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s (Expidus %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, expidus_version_string ());
        g_print("%s\n", "Copyright (c) 2004-2020");
        g_print("\t%s\n\n", _("The Expidus development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");

        return EXIT_SUCCESS;
    }

    if(G_UNLIKELY(!esconf_init(&error))) {
        expidus_dialog_show_error (NULL,
                                error,
                                _("Unable to contact settings server"));
        g_error_free(error);
        return EXIT_FAILURE;
    }

    gtk_window_set_default_icon_name("expidus1-session");

    /* hook to make sure the libexpidus1ui library is linked */
    if (expidus_titled_dialog_get_type() == 0)
        return EXIT_FAILURE;

    builder = gtk_builder_new();
    gtk_builder_add_from_string(builder,
                                expidus1_session_settings_ui,
                                expidus1_session_settings_ui_length,
                                &error);

    if(!builder) {
        if (error) {
            expidus_dialog_show_error(NULL, error,
                _("Unable to create user interface from embedded definition data"));
            g_error_free (error);
        }
        return EXIT_FAILURE;
    }

    session_editor_init(builder);

    channel = esconf_channel_get (SETTINGS_CHANNEL);

    /* FIXME: someday, glade-ify this, maybe. */
    esae_page = esae_window_new();
    gtk_widget_show(esae_page);
    notebook = GTK_WIDGET(gtk_builder_get_object(builder, "plug-child"));
    lbl = gtk_label_new_with_mnemonic(_("App_lication Autostart"));
    gtk_widget_show(lbl);
    gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), esae_page, lbl, 1);

    label_active_session = GTK_WIDGET (gtk_builder_get_object (builder, "label_active_session"));
    active_session = esconf_channel_get_string (channel, "/general/SessionName", "Default");
    active_session_label = _("Currently active session:");
    format = "%s <b>%s</b>";
    markup = g_markup_printf_escaped (format, active_session_label, active_session);
    gtk_label_set_markup (GTK_LABEL (label_active_session), markup);
    g_free (markup);

    delete_button = gtk_builder_get_object (builder, "btn_delete_session");
    treeview = gtk_builder_get_object (builder, "saved-sessions-list");
    g_signal_connect (delete_button, "clicked", G_CALLBACK (settings_list_sessions_delete_session), GTK_TREE_VIEW (treeview));

    /* Check if there are saved sessions and if so, show the "Saved Sessions" tab */
    rc = settings_list_sessions_open_rc ();
    if (rc)
        visible = TRUE;
    else
        visible = FALSE;
    expidus1_session_settings_show_saved_sessions (builder, rc, visible);

    /* bind widgets to esconf */
    esconf_g_property_bind(channel, "/chooser/AlwaysDisplay", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_display_chooser"),
                           "active");
    esconf_g_property_bind(channel, "/general/AutoSave", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_session_autosave"),
                           "active");
    esconf_g_property_bind(channel, "/general/PromptOnLogout", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_logout_prompt"),
                           "active");
    esconf_g_property_bind(channel, "/compat/LaunchGNOME", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_compat_gnome"),
                           "active");
    esconf_g_property_bind(channel, "/compat/LaunchKDE", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_compat_kde"),
                           "active");
    esconf_g_property_bind(channel, "/security/EnableTcp", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_enable_tcp"),
                           "active");
    esconf_g_property_bind(channel, "/shutdown/LockScreen", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_lock_screen"),
                           "active");

    if(G_UNLIKELY(opt_socket_id == 0)) {
        GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "expidus1_session_settings_dialog"));

        g_signal_connect(dialog, "response", G_CALLBACK(expidus1_session_settings_dialog_response), NULL);
        g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_main_quit), NULL);

        gtk_widget_show(dialog);

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id ("FAKE ID");

        gtk_main();
    } else {
        GtkWidget *plug;
        GtkWidget *plug_child;
        GtkWidget *parent;

        plug_child = GTK_WIDGET(gtk_builder_get_object(builder, "plug-child"));
        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show (plug);

        parent = gtk_widget_get_parent (plug_child);
        if (parent)
        {
            g_object_ref (plug_child);
            gtk_container_remove (GTK_CONTAINER (parent), plug_child);
            gtk_container_add (GTK_CONTAINER (plug), plug_child);
            g_object_unref (plug_child);
        }
        g_signal_connect(plug, "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        /* Stop startup notification */
        gdk_notify_startup_complete();

        gtk_main();
    }

    g_object_unref(builder);

    return EXIT_SUCCESS;
}
