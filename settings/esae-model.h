/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@expidus.org>
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

#ifndef __ESAE_MODEL_H__
#define __ESAE_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _EsaeModelClass EsaeModelClass;
typedef struct _EsaeModel      EsaeModel;

#define ESAE_TYPE_MODEL             (esae_model_get_type ())
#define ESAE_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ESAE_TYPE_MODEL, EsaeModel))
#define ESAE_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ESAE_TYPE_MODEL, EsaeModelClass))
#define ESAE_IS_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ESAE_TYPE_MODEL))
#define ESAE_IS_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ESAE_TYPE_MODEL))
#define ESAE_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ESAE_TYPE_MODEL, EsaeModelClass))

#define ESSM_TYPE_RUN_HOOK (essm_run_hook_get_type ())

/**
 * EssmRunHook:
 * The trigger / hook on which a specific command should be executed
 **/
typedef enum
{
  ESSM_RUN_HOOK_LOGIN,
  ESSM_RUN_HOOK_LOGOUT,
  ESSM_RUN_HOOK_SHUTDOWN,
  ESSM_RUN_HOOK_RESTART,
  ESSM_RUN_HOOK_SUSPEND,
  ESSM_RUN_HOOK_HIBERNATE,
  ESSM_RUN_HOOK_HYBRID_SLEEP,
  ESSM_RUN_HOOK_SWITCH_USER,
} EssmRunHook;

GType essm_run_hook_get_type (void) G_GNUC_CONST;

/**
 * EsaeModelColumn:
 *
 * Columns exported by the #EsaeModelColumn using
 * the #GtkTreeModel interface.
 **/
typedef enum
{
  ESAE_MODEL_COLUMN_ICON,
  ESAE_MODEL_COLUMN_NAME,
  ESAE_MODEL_COLUMN_ENABLED,
  ESAE_MODEL_COLUMN_REMOVABLE,
  ESAE_MODEL_COLUMN_TOOLTIP,
  ESAE_MODEL_RUN_HOOK,
  ESAE_MODEL_N_COLUMNS,
} EsaeModelColumn;

GType         esae_model_get_type (void) G_GNUC_CONST;

GtkTreeModel *esae_model_new      (void);

gboolean      esae_model_add      (EsaeModel    *model,
                                   const gchar  *name,
                                   const gchar  *description,
                                   const gchar  *command,
                                   EssmRunHook   run_hook,
                                   GError      **error);

gboolean      esae_model_get      (EsaeModel    *model,
                                   GtkTreeIter  *iter,
                                   gchar       **name,
                                   gchar       **description,
                                   gchar       **command,
                                   EssmRunHook  *run_hook,
                                   GError      **error);

gboolean      esae_model_remove   (EsaeModel    *model,
                                   GtkTreeIter  *iter,
                                   GError      **error);

gboolean      esae_model_edit     (EsaeModel    *model,
                                   GtkTreeIter  *iter,
                                   const gchar  *name,
                                   const gchar  *description,
                                   const gchar  *command,
                                   EssmRunHook   run_hook,
                                   GError      **error);

gboolean      esae_model_toggle   (EsaeModel    *model,
                                   GtkTreeIter  *iter,
                                   GError      **error);

gboolean esae_model_set_run_hook  (GtkTreeModel *tree_model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   EssmRunHook   run_hook,
                                   GError      **error);

G_END_DECLS;

#endif /* !__ESAE_MODEL_H__ */
