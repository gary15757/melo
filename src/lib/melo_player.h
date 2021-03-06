/*
 * melo_player.h: Player base class
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

#ifndef __MELO_PLAYER_H__
#define __MELO_PLAYER_H__

#include <glib-object.h>

#include "melo_playlist.h"
#include "melo_tags.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER             (melo_player_get_type ())
#define MELO_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER, MeloPlayer))
#define MELO_IS_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER))
#define MELO_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER, MeloPlayerClass))
#define MELO_IS_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER))
#define MELO_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER, MeloPlayerClass))

typedef struct _MeloPlayer MeloPlayer;
typedef struct _MeloPlayerClass MeloPlayerClass;
typedef struct _MeloPlayerPrivate MeloPlayerPrivate;

typedef struct _MeloPlayerInfo MeloPlayerInfo;
typedef struct _MeloPlayerStatus MeloPlayerStatus;
typedef struct _MeloPlayerStatusPrivate MeloPlayerStatusPrivate;

/**
 * MeloPlayerState:
 * @MELO_PLAYER_STATE_NONE: Player has no media loaded
 * @MELO_PLAYER_STATE_LOADING: Loading a new media (the media informations have
 *    yet been completely retrieved: it can occur with a file from the network)
 * @MELO_PLAYER_STATE_BUFFERING: Buffering the media (the media is loaded but
 *    the player is buffering data before playing some samples)
 * @MELO_PLAYER_STATE_PLAYING: Playing the media
 * @MELO_PLAYER_STATE_PAUSED_LOADING: Loading the media in paused state
 * @MELO_PLAYER_STATE_PAUSED_BUFFERING: Buffering the media in paused state
 * @MELO_PLAYER_STATE_PAUSED: Media is paused
 * @MELO_PLAYER_STATE_STOPPED: Media is stopped
 * @MELO_PLAYER_STATE_ERROR: An error occurred during one of previous states
 * @MELO_PLAYER_STATE_COUNT: Number of state type
 *
 * #MeloPlayerState indicates the current state of a #MeloPlayer. It is used to
 * know if the player is loading or buffering (especially for remote network
 * media), playing or waiting to play.
 */
typedef enum {
  MELO_PLAYER_STATE_NONE,
  MELO_PLAYER_STATE_LOADING,
  MELO_PLAYER_STATE_BUFFERING,
  MELO_PLAYER_STATE_PLAYING,
  MELO_PLAYER_STATE_PAUSED_LOADING,
  MELO_PLAYER_STATE_PAUSED_BUFFERING,
  MELO_PLAYER_STATE_PAUSED,
  MELO_PLAYER_STATE_STOPPED,
  MELO_PLAYER_STATE_ERROR,

  MELO_PLAYER_STATE_COUNT,
} MeloPlayerState;

/**
 * MeloPlayer:
 *
 * The opaque #MeloPlayer data structure.
 */
struct _MeloPlayer {
  GObject parent_instance;

  /*< protected >*/
  MeloPlaylist *playlist;

  /*< private >*/
  MeloPlayerPrivate *priv;
};

/**
 * MeloPlayerClass:
 * @parent_class: Object parent class
 * @get_info: Provide the #MeloPlayerInfo defined by the #MeloPlayer
 * @add: Add a media by path to the player (and then playlist if used)
 * @load: Load a media by path with the player in pause / stopped state
 * @play: Play a media by path with the player
 * @set_state: Set player state (playing / paused / stopped)
 * @prev: Play previous media in playlist
 * @next: Play nest media in playlist
 * @set_pos: Seek in media stream (in ms)
 * @set_volume: Set the volume of the player
 * @set_mute: Set the player mute state
 * @get_pos: Get current position in stream (in ms)
 *
 * Subclasses must override at least the get_info virtual method. Others can be
 * kept undefined but functionalities will be reduced.
 */
struct _MeloPlayerClass {
  GObjectClass parent_class;

  const MeloPlayerInfo *(*get_info) (MeloPlayer *player);

  gboolean (*add) (MeloPlayer *player, const gchar *path, const gchar *name,
                   MeloTags *tags);
  gboolean (*load) (MeloPlayer *player, const gchar *path, const gchar *name,
                    MeloTags *tags, gboolean insert, gboolean stopped);
  gboolean (*play) (MeloPlayer *player, const gchar *path, const gchar *name,
                    MeloTags *tags, gboolean insert);
  MeloPlayerState (*set_state) (MeloPlayer *player, MeloPlayerState state);
  gboolean (*prev) (MeloPlayer *player);
  gboolean (*next) (MeloPlayer *player);
  gint (*set_pos) (MeloPlayer *player, gint pos);
  gdouble (*set_volume) (MeloPlayer *player, gdouble volume);
  gboolean (*set_mute) (MeloPlayer *player, gboolean mute);

  gint (*get_pos) (MeloPlayer *player);
};

/**
 * MeloPlayerInfo:
 * @name: the display name of the #MeloPlayer
 * @playlist_id: the ID of the #MeloPlaylist attached to the player
 *
 * #MeloPlayerInfo provides all details on a #MeloPlayer instance as its name,
 * description, capabilities, ... It is important to define this structure in
 * order to give a correct feedback for user.
 */
struct _MeloPlayerInfo {
  const gchar *name;
  const gchar *playlist_id;
  struct {
    gboolean state;
    gboolean prev;
    gboolean next;
    gboolean volume;
    gboolean mute;
  } control;
};

/**
 * MeloPlayerStatus:
 * @state: current state of the player, see #MeloPlayerState
 * @buffer_percent: buffering percentage when @state is
 *    MELO_PLAYER_STATE_BUFFERING or MELO_PLAYER_STATE_PAUSED_BUFFERING
 * @pos: current position of the stream (in ms)
 * @duration: duration of the current media (in ms)
 * @has_prev: a media is available before the current one in playlist
 * @has_next: a media is available after the current one in playlist
 * @volume: current volume
 * @mute: current mute state
 *
 * #MeloPlayerStatus handles all details about the current status of the
 * player and the media its playing. Some other informations are provided by the
 * #MeloPlayerStatus which are:
 *  - a #MeloTags of the current media which can be retrieved with
 *    melo_player_status_get_tags()
 *
 * All the data handled by the #MeloPlayerStatus can also be retrieved directly
 * from the #MeloPlayer instance.
 */
struct _MeloPlayerStatus {
  MeloPlayerState state;
  gint buffer_percent;
  gint pos;
  gint duration;
  gboolean has_prev;
  gboolean has_next;
  gdouble volume;
  gboolean mute;

  /*< private >*/
  MeloPlayerStatusPrivate *priv;
};

GType melo_player_get_type (void);

MeloPlayer *melo_player_new (GType type, const gchar *id, const gchar *name);
const gchar *melo_player_get_id (MeloPlayer *player);
const gchar *melo_player_get_name (MeloPlayer *player);
const MeloPlayerInfo *melo_player_get_info (MeloPlayer *player);
MeloPlayer *melo_player_get_player_by_id (const gchar *id);
GList *melo_player_get_list (void);

/* Playlist */
void melo_player_set_playlist (MeloPlayer *player, MeloPlaylist *playlist);
MeloPlaylist *melo_player_get_playlist (MeloPlayer *player);

/* Player control */
gboolean melo_player_add (MeloPlayer *player, const gchar *path,
                          const gchar *name, MeloTags *tags);
gboolean melo_player_load (MeloPlayer *player, const gchar *path,
                           const gchar *name, MeloTags *tags, gboolean insert,
                           gboolean stopped);
gboolean melo_player_play (MeloPlayer *player, const gchar *path,
                           const gchar *name, MeloTags *tags, gboolean insert);
MeloPlayerState melo_player_set_state (MeloPlayer *player,
                                       MeloPlayerState state);
gboolean melo_player_prev (MeloPlayer *player);
gboolean melo_player_next (MeloPlayer *player);
gint melo_player_set_pos (MeloPlayer *player, gint pos);
gdouble melo_player_set_volume (MeloPlayer *player, gdouble volume);
gboolean melo_player_set_mute (MeloPlayer *player, gboolean mute);

/* Player status */
MeloPlayerStatus *melo_player_get_status (MeloPlayer *player,
                                          gint64 *timestamp);
MeloPlayerState melo_player_get_state (MeloPlayer *player);
gchar *melo_player_get_media_name (MeloPlayer *player);
gint melo_player_get_pos (MeloPlayer *player);
gdouble melo_player_get_volume (MeloPlayer *player);
gboolean melo_player_get_mute (MeloPlayer *player);
MeloTags *melo_player_get_tags (MeloPlayer *player);

/* Protected functions for Player status update */
gboolean melo_player_reset_status (MeloPlayer *player, MeloPlayerState state,
                                   const gchar *name, MeloTags *tags);
void melo_player_set_status_state (MeloPlayer *player, MeloPlayerState state);
void melo_player_set_status_buffering (MeloPlayer *player,
                                       MeloPlayerState state, guint percent);
void melo_player_set_status_pos (MeloPlayer *player, gint pos);
void melo_player_set_status_duration (MeloPlayer *player, gint duration);
void melo_player_set_status_playlist (MeloPlayer *player, gboolean has_prev,
                                      gboolean has_next);
void melo_player_set_status_volume (MeloPlayer *player, gdouble volume);
void melo_player_set_status_mute (MeloPlayer *player, gboolean mute);
void melo_player_set_status_name (MeloPlayer *player, const gchar *name);
void melo_player_set_status_error (MeloPlayer *player, const gchar *error);
void melo_player_set_status_tags (MeloPlayer *player, MeloTags *tags);
void melo_player_take_status_tags (MeloPlayer *player, MeloTags *tags);

/* MeloPlayerStatus functions */
MeloPlayerStatus *melo_player_status_ref (MeloPlayerStatus *status);
void melo_player_status_unref (MeloPlayerStatus *status);
gchar *melo_player_status_get_name (const MeloPlayerStatus *status);
gchar *melo_player_status_get_error (const MeloPlayerStatus *status);
MeloTags *melo_player_status_get_tags (const MeloPlayerStatus *status);

/* Advanced getter for MeloPlayerStatus (use with lock/unlock) */
void melo_player_status_lock (const MeloPlayerStatus *status);
void melo_player_status_unlock (const MeloPlayerStatus *status);
const gchar *melo_player_status_lock_get_name (const MeloPlayerStatus *status);
const gchar *melo_player_status_lock_get_error (const MeloPlayerStatus *status);

/* MeloPlayerState helpers */
const gchar *melo_player_state_to_string (MeloPlayerState state);
MeloPlayerState melo_player_state_from_string (const gchar *sstate);

G_END_DECLS

#endif /* __MELO_PLAYER_H__ */
