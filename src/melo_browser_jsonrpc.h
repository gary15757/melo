/*
 * melo_browser_jsonrpc.h: Browser base JSON-RPC interface
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

#ifndef __MELO_BROWSER_JSONRPC_H__
#define __MELO_BROWSER_JSONRPC_H__

#include "melo_browser.h"
#include "melo_jsonrpc.h"

typedef enum {
  MELO_BROWSER_JSONRPC_FIELDS_NONE = 0,
  MELO_BROWSER_JSONRPC_FIELDS_NAME = 1,
  MELO_BROWSER_JSONRPC_FIELDS_DESCRIPTION = 2,
  MELO_BROWSER_JSONRPC_FIELDS_FULL = 255,
} MeloBrowserJSONRPCFields;

MeloBrowserJSONRPCFields melo_browser_jsonrpc_get_fields (JsonObject *obj);
JsonObject *melo_browser_jsonrpc_info_to_object (
                                               const gchar *id,
                                               const MeloBrowserInfo *info,
                                               MeloBrowserJSONRPCFields fields);
#endif /* __MELO_BROWSER_JSONRPC_H__ */
