/*
 * melo_browser_file.c: File Browser using GIO
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <gio/gio.h>

#include "melo_browser_file.h"

#define MELO_BROWSER_FILE_ID "melo_browser_file_id"

/* File browser info */
static MeloBrowserInfo melo_browser_file_info = {
  .name = "Browse files",
  .description = "Navigate though local and remote filesystems",
};

static gint vms_cmp (GObject *a, GObject *b);
static void vms_added(GVolumeMonitor *monitor, GObject *obj,
                      MeloBrowserFilePrivate *priv);
static void vms_removed(GVolumeMonitor *monitor, GObject *obj,
                        MeloBrowserFilePrivate *priv);
static void melo_browser_file_set_id (GObject *obj,
                                      MeloBrowserFilePrivate *priv);
static const MeloBrowserInfo *melo_browser_file_get_info (MeloBrowser *browser);
static GList *melo_browser_file_get_list (MeloBrowser *browser,
                                          const gchar *path);

struct _MeloBrowserFilePrivate {
  GVolumeMonitor *monitor;
  GMutex mutex;
  GList *vms;
  GHashTable *ids;
};

G_DEFINE_TYPE_WITH_PRIVATE (MeloBrowserFile, melo_browser_file, MELO_TYPE_BROWSER)

static void
melo_browser_file_finalize (GObject *gobject)
{
  MeloBrowserFile *browser_file = MELO_BROWSER_FILE (gobject);
  MeloBrowserFilePrivate *priv =
                          melo_browser_file_get_instance_private (browser_file);

  /* Release volume monitor */
  g_object_unref (priv->monitor);

  /* Clear mutex */
  g_mutex_clear (&priv->mutex);

  /* Free hash table */
  g_hash_table_remove_all (priv->ids);
  g_hash_table_unref (priv->ids);

  /* Free volume and mount list */
  g_list_free_full (priv->vms, g_object_unref);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_browser_file_parent_class)->finalize (gobject);
}

static void
melo_browser_file_class_init (MeloBrowserFileClass *klass)
{
  MeloBrowserClass *bclass = MELO_BROWSER_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  bclass->get_info = melo_browser_file_get_info;
  bclass->get_list = melo_browser_file_get_list;

  /* Add custom finalize() function */
  oclass->finalize = melo_browser_file_finalize;
}

static void
melo_browser_file_init (MeloBrowserFile *self)
{
  MeloBrowserFilePrivate *priv = melo_browser_file_get_instance_private (self);

  self->priv = priv;

  /* Get volume monitor */
  priv->monitor = g_volume_monitor_get ();

  /* Init mutex */
  g_mutex_init (&priv->mutex);

  /* Get list of volumes and mounts and sort by name */
  priv->vms = g_list_concat (g_volume_monitor_get_volumes (priv->monitor),
                             g_volume_monitor_get_mounts (priv->monitor));
  priv->vms = g_list_sort (priv->vms, (GCompareFunc) vms_cmp);

  /* Init Hash table for IDs */
  priv->ids = g_hash_table_new (g_str_hash, g_str_equal);

  /* Generate id for each volumes and mounts */
  g_list_foreach (priv->vms, (GFunc) melo_browser_file_set_id, priv);

  /* Subscribe to volume and mount events of volume monitor */
  g_signal_connect (priv->monitor, "volume-added",
                    (GCallback) vms_added, priv);
  g_signal_connect (priv->monitor, "volume-removed",
                    (GCallback) vms_removed, priv);
  g_signal_connect (priv->monitor, "mount_added",
                    (GCallback) vms_added, priv);
  g_signal_connect (priv->monitor, "mount_removed",
                    (GCallback) vms_removed, priv);
}

static const MeloBrowserInfo *
melo_browser_file_get_info (MeloBrowser *browser)
{
  return &melo_browser_file_info;
}

static void
melo_browser_file_set_id (GObject *obj, MeloBrowserFilePrivate *priv)
{
  gchar *sha1, *id;

  /* Calculate sha1 from GVolume or GMount instance pointer */
  sha1 = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
                                      (const guchar *) &obj, sizeof (obj));

  /* Keep only 8 first characters to create ID */
  id = g_strndup (sha1, 8);
  g_free (sha1);

  /* Add to object and internal hash table */
  g_object_set_data_full (obj, MELO_BROWSER_FILE_ID, id, g_free);
  g_hash_table_insert (priv->ids, id, obj);
}

static gint
vms_cmp (GObject *a, GObject *b)
{
  gchar *na, *nb;
  gint ret;

  /* Get name for object a */
  if (G_IS_VOLUME (a))
    na = g_volume_get_name (G_VOLUME (a));
  else
    na = g_mount_get_name (G_MOUNT (a));

  /* Get name for object b */
  if (G_IS_VOLUME (b))
    nb = g_volume_get_name (G_VOLUME (b));
  else
    nb = g_mount_get_name (G_MOUNT (b));

  /* Compare names */
  ret = g_strcmp0 (na, nb);
  g_free (na);
  g_free (nb);

  return ret;
}

static void
vms_added(GVolumeMonitor *monitor, GObject *obj, MeloBrowserFilePrivate *priv)
{
  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Add to volume / mount list and set id to object */
  priv->vms = g_list_insert_sorted (priv->vms, g_object_ref (obj),
                                    (GCompareFunc) vms_cmp);
  melo_browser_file_set_id (obj, priv);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
vms_removed(GVolumeMonitor *monitor, GObject *obj, MeloBrowserFilePrivate *priv)
{
  const gchar *id;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Remove from hash table */
  id = g_object_get_data (obj, MELO_BROWSER_FILE_ID);
  g_hash_table_remove (priv->ids, id);

  /* Remove from volume / mount list */
  priv->vms = g_list_remove (priv->vms, obj);
  g_object_unref (obj);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);
}

static void
mount_ready (GObject *obj, GAsyncResult *res, gpointer user_data)
{
  GMutex *mutex = user_data;

  /* Signal end of mount task */
  g_mutex_unlock (mutex);
}

static const gchar *
melo_brower_file_fix_path (const gchar *path)
{
  while (*path != '\0' && *path == '/')
    path++;
  return path;
}

static GList *
melo_browser_file_list (GFile *dir)
{
  GFileEnumerator *dir_enum;
  GFileInfo *info;
  GList *list = NULL;

  /* Get details */
  if (g_file_query_file_type (dir, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  /* Get list of directory */
  dir_enum = g_file_enumerate_children (dir,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME,
                                    0, NULL, NULL);
  if (!dir_enum)
    return NULL;

  /* Create list */
  while ((info = g_file_enumerator_next_file (dir_enum, NULL, NULL))) {
    MeloBrowserItem *item;
    const gchar *itype;
    GFileType type;

    /* Get item type */
    type = g_file_info_get_file_type (info);
    if (type == G_FILE_TYPE_REGULAR)
      itype = "file";
    else if (type == G_FILE_TYPE_DIRECTORY)
      itype = "directory";
    else {
      g_object_unref (info);
      continue;
    }

    /* Create a new browser item and insert in list */
    item = melo_browser_item_new (g_file_info_get_name (info), itype);
    item->full_name = g_strdup (g_file_info_get_display_name (info));
    list = g_list_insert_sorted (list, item,
                                 (GCompareFunc) melo_browser_item_cmp);
    g_object_unref (info);
  }
  g_object_unref (dir_enum);

  return list;
}

static GList *
melo_browser_file_get_local_list (const gchar *uri)
{
  GFile *dir;
  GList *list;

  /* Open directory */
  dir = g_file_new_for_uri (uri);
  if (!dir)
    return NULL;

  /* Get list from GFile */
  list = melo_browser_file_list (dir);
  g_object_unref (dir);

  return list;
}

static GList *
melo_browser_file_get_volume_list (MeloBrowserFile *bfile, const gchar *path)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GMount *mount = NULL;
  GFile *root, *dir;
  GList *list;

  GObject *obj;
  gchar *id;

  /* Extract volume / mount id from path */
  id = g_strstr_len (path, -1, "/");
  if (id) {
    gsize len = id - path;
    id = g_strndup (path, len);
    path += len + 1;
  } else {
    id = g_strdup (path);
    path = "";
  }

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Get volume / mount from hash table */
  obj = g_hash_table_lookup (priv->ids, id);
  g_free (id);

  /* Volume / mount id not found */
  if (!obj) {
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
  g_object_ref (obj);

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);

  /* Extract mount from object */
  if (G_IS_VOLUME (obj)) {
    GVolume *vol = G_VOLUME (obj);

    /* Get mount */
    mount = g_volume_get_mount (vol);
    if (!mount) {
      GMutex mutex;

      /* Mount volume */
      g_mutex_init (&mutex);
      g_mutex_lock (&mutex);
      g_volume_mount (vol, 0, NULL, NULL, mount_ready, &mutex);

      /* Wait for end of mount operation and get GMount */
      g_mutex_lock (&mutex);
      mount = g_volume_get_mount (vol);
      g_mutex_unlock (&mutex);
      g_mutex_clear (&mutex);
    }

    g_object_unref (vol);
  } else
    mount = G_MOUNT (obj);

  /* Volume doesn't exist */
  if (!mount)
    return NULL;

  /* Get root */
  root = g_mount_get_root (mount);
  if (!root) {
    g_object_unref (mount);
    return NULL;
  }
  g_object_unref (mount);

  /* Get final directory from our path */
  dir = g_file_resolve_relative_path (root, path);
  if (!dir) {
    g_object_unref (root);
    return NULL;
  }
  g_object_unref (root);

  /* List files from our GFile  */
  list = melo_browser_file_list (dir);
  g_object_unref (dir);

  return list;
}

static GList *
melo_browser_file_list_volumes (MeloBrowserFile *bfile, GList *list)
{
  MeloBrowserFilePrivate *priv = bfile->priv;
  GList *l;

  /* Lock volume/mount list */
  g_mutex_lock (&priv->mutex);

  /* Get volume / mount list */
  for (l = priv->vms; l != NULL; l = l->next) {
    GObject *obj = G_OBJECT (l->data);
    MeloBrowserItem *item;
    gchar *full_name, *id;
    GVolume *vol;
    GMount *mnt;

    /* Get mount if possible or volume */
    if (G_IS_VOLUME (obj)) {
      vol = G_VOLUME (obj);

      /* Get mount */
      mnt = g_volume_get_mount (vol);
      if (!mnt)
        g_object_ref (vol);
    } else {
      mnt = G_MOUNT (obj);

      /* Skip if mount has a volume */
      vol = g_mount_get_volume (mnt);
      if (vol) {
        g_object_unref (vol);
        continue;
      } else
        g_object_ref (mnt);
    }

    /* Get id and full name */
    if (mnt) {
      full_name = g_mount_get_name (mnt);
      id = g_strdup (g_object_get_data (G_OBJECT (mnt), MELO_BROWSER_FILE_ID));
      g_object_unref (mnt);
    } else {
      full_name = g_volume_get_name (vol);
      id = g_strdup (g_object_get_data (G_OBJECT (vol), MELO_BROWSER_FILE_ID));
      g_object_unref (vol);
    }

    /* Create item */
    item = melo_browser_item_new (NULL, "category");
    item->name = id;
    item->full_name = full_name;
    list = g_list_append(list, item);
  }

  /* Unlock volume/mount list */
  g_mutex_unlock (&priv->mutex);

  return list;
}

static GList *
melo_browser_file_get_list (MeloBrowser *browser, const gchar *path)
{
  MeloBrowserFile *bfile = MELO_BROWSER_FILE (browser);
  GList *list = NULL;
  gchar *uri;

  /* Check path */
  if (!path || *path != '/')
    return NULL;
  path++;

  /* Parse path and select list action */
  if (*path == '\0') {
    /* Root path: "/" */
    MeloBrowserItem *item;

    /* Add Local entry for local file system */
    item = melo_browser_item_new ("local", "category");
    item->full_name = g_strdup ("Local");
    list = g_list_append(list, item);

    /* Add local volumes to list */
    list = melo_browser_file_list_volumes (bfile, list);
  } else if (g_str_has_prefix (path, "local")) {
    /* Local path: "/local/" */
    path = melo_brower_file_fix_path (path + 5);
    uri = g_strdup_printf ("file:/%s", path);
    list = melo_browser_file_get_local_list (uri);
    g_free (uri);
  } else {
    /* Volume path: "/VOLUME_ID/" */
    list = melo_browser_file_get_volume_list (bfile, path);
  }

  return list;
}
