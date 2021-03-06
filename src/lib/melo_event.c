/*
 * melo_event.c: Event dispatcher
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#include "melo_event.h"

/**
 * SECTION:melo_event
 * @title: MeloEvent
 * @short_description: Event generator and manager for Melo
 *
 * The MeloEvent module is intended to provide event generator for Melo objects
 * and a client interface to catch and parse easily all events generated by
 * Melo object instances.
 *
 * All basic event types (for #MeloModule, #MeloBrowser, #MeloPlayer and
 * #MeloPlaylist are already used in base class implementation and should not be
 * used by final developer, except for specific usage. However, the
 * #MELO_EVENT_TYPE_GENERAL can be used for any custom or global events and are
 * executable specific. No generator or parser are then provided for this event
 * type.
 *
 * To catch events, two functions are provided: one to register a callback to
 * handle new events, named melo_event_register() and another one to unregister
 * and destroy the client instance, named melo_event_unregister().
 *
 * In the callback, the event type must be used to determine which sub-type to
 * use and then which parser to use to convert the opaque data pointer to a
 * comprehensible information.
 * The callback is not threaded and long operation or blocking calls should not
 * be done in a callback implementation.
 */

/* Event client list */
G_LOCK_DEFINE_STATIC (melo_event_mutex);
static GList *melo_event_clients = NULL;

struct _MeloEventClient {
  MeloEventCallback callback;
  gpointer user_data;
};

typedef struct {
  MeloPlayerState state;
  guint percent;
} MeloEventPlayerBuffering;

typedef struct {
  gboolean has_prev;
  gboolean has_next;
} MeloEventPlayerPlaylist;

/**
 * melo_event_register:
 * @callback: a function to call for new events
 * @user_data: a pointer to associate with @callback
 *
 * Create and register a new event client to receive and parse events coming
 * from Melo objects.
 *
 * Returns: (transfer full): a new #MeloEventClient instance or %NULL if failed.
 */
MeloEventClient *
melo_event_register (MeloEventCallback callback, gpointer user_data)
{
  MeloEventClient *client;

  /* Create new client context */
  client = g_slice_new0 (MeloEventClient);
  if (!client)
    return NULL;

  /* Fill client context */
  client->callback = callback;
  client->user_data = user_data;

  /* Add client to list */
  G_LOCK (melo_event_mutex);
  melo_event_clients = g_list_prepend (melo_event_clients, client);
  G_UNLOCK (melo_event_mutex);

  return client;
}

/**
 * melo_event_unregister:
 * @client: an event client
 *
 * Unregister and destroy an event client.
 */
void
melo_event_unregister (MeloEventClient *client)
{
  /* Remove from list */
  G_LOCK (melo_event_mutex);
  melo_event_clients = g_list_remove (melo_event_clients, client);
  G_UNLOCK (melo_event_mutex);

  /* Free client */
  g_slice_free (MeloEventClient, client);
}

static const gchar *melo_event_type_string[] = {
  [MELO_EVENT_TYPE_GENERAL] = "general",
  [MELO_EVENT_TYPE_MODULE] = "module",
  [MELO_EVENT_TYPE_BROWSER] = "browser",
  [MELO_EVENT_TYPE_PLAYER] = "player",
  [MELO_EVENT_TYPE_PLAYLIST] = "playlist",
};

/**
 * melo_event_type_to_string:
 * @type: an event type
 *
 * Convert a #MeloEventType to a string.
 *
 * Returns: a string with the translated #MeloEventType, %NULL otherwise.
 */
const gchar *
melo_event_type_to_string (MeloEventType type)
{
  if (type < MELO_EVENT_TYPE_COUNT)
    return melo_event_type_string[type];
  return NULL;
}

/**
 * melo_event_new:
 * @type: the event type
 * @event: the sub-type of the event
 * @id: the ID of the object instance
 * @data: the event data
 * @free_data_func: a function called to free @data
 *
 * Create a new event and forward it to all registered clients. This function
 * should be used only for custom or global events with
 * #MELO_EVENT_TYPE_GENERAL. For other event types, please consider using
 * function already defined.
 */
void
melo_event_new (MeloEventType type, guint event, const gchar *id, gpointer data,
                GDestroyNotify free_data_func)
{
  GList *l;

  /* Lock client list */
  G_LOCK (melo_event_mutex);

  /* Send event to all registered clients */
  for (l = melo_event_clients; l != NULL; l = l->next) {
    MeloEventClient *client = (MeloEventClient *) l->data;

    /* Call event callback */
    client->callback(client, type, event, id, data, client->user_data);
  }

  /* Unlock client list */
  G_UNLOCK (melo_event_mutex);

  /* Free event data */
  if (free_data_func)
    free_data_func (data);
}

#define melo_event_player(event, id, data, free) \
  melo_event_new (MELO_EVENT_TYPE_PLAYER, MELO_EVENT_PLAYER_##event, id, data, \
                  free)

/**
 * melo_event_player_new:
 * @id: the #MeloPlayer ID
 * @info: the #MeloPlayerInfo of the player
 *
 * A new player has been created with the ID @id.
 */
void
melo_event_player_new (const gchar *id, const MeloPlayerInfo *info)
{
  melo_event_player (NEW, id, (gpointer) info, NULL);
}

/**
 * melo_event_player_delete:
 * @id: the #MeloPlayer ID
 *
 * The player identified by @id has been destroyed.
 */
void
melo_event_player_delete (const gchar *id)
{
  melo_event_player (DELETE, id, NULL, NULL);
}

/**
 * melo_event_player_status:
 * @id: the #MeloPlayer ID
 * @status: the new player status
 *
 * The player status has been updated.
 */
void
melo_event_player_status (const gchar *id, MeloPlayerStatus *status)
{
  melo_event_player (STATUS, id, status,
                     (GDestroyNotify) melo_player_status_unref);
}

/**
 * melo_event_player_state:
 * @id: the #MeloPlayer ID
 * @state: the new player state
 *
 * The player state has changed.
 */
void
melo_event_player_state (const gchar *id, MeloPlayerState state)
{
  melo_event_player (STATE, id, &state, NULL);
}

/**
 * melo_event_player_buffering:
 * @id: the #MeloPlayer ID
 * @state: the new player state
 * @percent: the new buffering percentage
 *
 * The buffering state of the player has changed.
 */
void
melo_event_player_buffering (const gchar *id, MeloPlayerState state,
                             guint percent)
{
  MeloEventPlayerBuffering evt = { .state = state, .percent = percent };
  melo_event_player (BUFFERING, id, &evt, NULL);
}

/**
 * melo_event_player_seek:
 * @id: the #MeloPlayer ID
 * @pos: the new position in the media stream
 *
 * A seek has been done on the media of the player.
 */
void
melo_event_player_seek (const gchar *id, gint pos)
{
  melo_event_player (SEEK, id, GINT_TO_POINTER (pos), NULL);
}

/**
 * melo_event_player_duration:
 * @id: the #MeloPlayer ID
 * @duration: the new media duration
 *
 * The media duration of the player has changed.
 */
void
melo_event_player_duration (const gchar *id, gint duration)
{
  melo_event_player (DURATION, id, GINT_TO_POINTER (duration), NULL);
}

/**
 * melo_event_player_playlist:
 * @id: the #MeloPlayer ID
 * @has_prev: the new previous in playlist flag
 * @has_next: the new next in playlist flag
 *
 * The playlist of the player has been updated.
 */
void
melo_event_player_playlist (const gchar *id, gboolean has_prev,
                            gboolean has_next)
{
  MeloEventPlayerPlaylist evt = { .has_prev = has_prev, .has_next = has_next };
  melo_event_player (PLAYLIST, id, &evt, NULL);
}

/**
 * melo_event_player_volume:
 * @id: the #MeloPlayer ID
 * @volume: the new volume value
 *
 * The volume value has changed.
 */
void
melo_event_player_volume (const gchar *id, gdouble volume)
{
  melo_event_player (VOLUME, id, &volume, NULL);
}

/**
 * melo_event_player_mute:
 * @id: the #MeloPlayer ID
 * @mute: the new mute value
 *
 * The mute value has changed.
 */
void
melo_event_player_mute (const gchar *id, gboolean mute)
{
  melo_event_player (MUTE, id, &mute, NULL);
}

/**
 * melo_event_player_name:
 * @id: the #MeloPlayer ID
 * @name: the new status name
 *
 * The status name of the player has changed.
 */
void
melo_event_player_name (const gchar *id, const gchar *name)
{
  melo_event_player (NAME, id, (gpointer) name, NULL);
}

/**
 * melo_event_player_error:
 * @id: the #MeloPlayer ID
 * @error: the error string
 *
 * An error occurred for the player.
 */
void
melo_event_player_error (const gchar *id, const gchar *error)
{
  melo_event_player (ERROR, id, (gpointer) error, NULL);
}

/**
 * melo_event_player_tags:
 * @id: the #MeloPlayer ID
 * @tags: the new #MeloTags
 *
 * The #MeloTags of the player has changed.
 */
void
melo_event_player_tags (const gchar *id, MeloTags *tags)
{
  melo_event_player (TAGS, id, tags, NULL);
}

/**
 * melo_event_player_new_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_NEW.
 *
 * Returns: (transfer none): a #MeloPlayerInfo containing the new informations
 * of the new player.
 */
const MeloPlayerInfo *
melo_event_player_new_parse (gpointer data)
{
  return (const MeloPlayerInfo *) data;
}

/**
 * melo_event_player_status_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_STATUS.
 *
 * Returns: (transfer none): a #MeloPlayerStatus containing the new status.
 */
MeloPlayerStatus *
melo_event_player_status_parse (gpointer data)
{
  return (MeloPlayerStatus *) data;
}

/**
 * melo_event_player_state_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_STATE.
 *
 * Returns: the new player state.
 */
MeloPlayerState
melo_event_player_state_parse (gpointer data)
{
  return *((MeloPlayerState *) data);
}

/**
 * melo_event_player_buffering_parse:
 * @data: the event data to parse
 * @state: a pointer to hold the new state, or %NULL
 * @percent: a pointer to hold the new percentage, or %NULL
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_BUFFERING.
 * The @state and @percent are set according to the new player state and the
 * new buffering percentage of the media.
 */
void
melo_event_player_buffering_parse (gpointer data, MeloPlayerState *state,
                                   guint *percent)
{
  MeloEventPlayerBuffering *evt = (MeloEventPlayerBuffering *) data;
  if (state)
    *state = evt->state;
  if (percent)
    *percent = evt->percent;
}

/**
 * melo_event_player_seek_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_SEEK.
 *
 * Returns: the new position in the media (in ms).
 */
gint
melo_event_player_seek_parse (gpointer data)
{
  return GPOINTER_TO_INT (data);
}

/**
 * melo_event_player_duration_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_DURATION.
 *
 * Returns: the new duration of the media (in ms).
 */
gint
melo_event_player_duration_parse (gpointer data)
{
  return GPOINTER_TO_INT (data);
}

/**
 * melo_event_player_playlist_parse:
 * @data: the event data to parse
 * @has_prev: a pointer to hold the previous in playlist flag, or %NULL
 * @has_next: a pointer to hold the next in playlist flag, or %NULL
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_PLAYLIST.
 * The @has_prev and @has_next are set according to the new status.
 */
void
melo_event_player_playlist_parse (gpointer data, gboolean *has_prev,
                                  gboolean *has_next)
{
  MeloEventPlayerPlaylist *evt = (MeloEventPlayerPlaylist *) data;
  if (has_prev)
    *has_prev = evt->has_prev;
  if (has_next)
    *has_next = evt->has_next;
}

/**
 * melo_event_player_volume_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_VOLUME.
 *
 * Returns: the new volume value.
 */
gdouble
melo_event_player_volume_parse (gpointer data)
{
  return *((gdouble *) data);
}

/**
 * melo_event_player_mute_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_MUTE.
 *
 * Returns: %TRUE if the mute has been enabled, %FALSE otherwise.
 */
gboolean
melo_event_player_mute_parse (gpointer data)
{
  return *((gboolean *) data);
}

/**
 * melo_event_player_name_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_NAME.
 *
 * Returns: a string containing the new status name.
 */
const gchar *
melo_event_player_name_parse (gpointer data)
{
  return (const gchar *) data;
}

/**
 * melo_event_player_error_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_ERROR.
 *
 * Returns: a string containing the error message.
 */
const gchar *
melo_event_player_error_parse (gpointer data)
{
  return (const gchar *) data;
}

/**
 * melo_event_player_tags_parse:
 * @data: the event data to parse
 *
 * Parse the event data for a #MELO_EVENT_PLAYER_TAGS.
 *
 * Returns: (transfer none): a #MeloTags containing the new tags.
 */
MeloTags *
melo_event_player_tags_parse (gpointer data)
{
  return (MeloTags *) data;
}

static const gchar *melo_event_player_string[] = {
  [MELO_EVENT_PLAYER_NEW] = "new",
  [MELO_EVENT_PLAYER_DELETE] = "delete",
  [MELO_EVENT_PLAYER_STATUS] = "status",
  [MELO_EVENT_PLAYER_STATE] = "state",
  [MELO_EVENT_PLAYER_BUFFERING] = "buffering",
  [MELO_EVENT_PLAYER_SEEK] = "seek",
  [MELO_EVENT_PLAYER_DURATION] = "duration",
  [MELO_EVENT_PLAYER_PLAYLIST] = "playlist",
  [MELO_EVENT_PLAYER_VOLUME] = "volume",
  [MELO_EVENT_PLAYER_MUTE] = "mute",
  [MELO_EVENT_PLAYER_NAME] = "name",
  [MELO_EVENT_PLAYER_ERROR] = "error",
  [MELO_EVENT_PLAYER_TAGS] = "tags",
};

/**
 * melo_event_player_to_string:
 * @event: a player sub-type event
 *
 * Convert a #MeloEventPlayer to a string.
 *
 * Returns: a string with the translated #MeloEventPlayer, %NULL otherwise.
 */
const gchar *
melo_event_player_to_string (MeloEventPlayer event)
{
  if (event < MELO_EVENT_PLAYER_COUNT)
    return melo_event_player_string[event];
  return NULL;
}
