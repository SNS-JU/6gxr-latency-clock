/* GStreamer
 * Copyright (C) 2024 Felician Nemeth <nemethf@tmit.bme.hu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GST_TIMESTAMPCOMMON_H_
#define _GST_TIMESTAMPCOMMON_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_FEC_SCHEME (gst_fec_scheme_get_type ())
GType gst_fec_scheme_get_type (void);

G_END_DECLS
#endif
