/* GStreamer
 * Copyright (C) 2016 William Manley <will@williammanley.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_TIMEOVERLAYPARSE_H_
#define _GST_TIMEOVERLAYPARSE_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <liquid.h>

G_BEGIN_DECLS

#define GST_TYPE_TIMEOVERLAYPARSE   (gst_timeoverlayparse_get_type())
#define GST_TIMEOVERLAYPARSE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIMEOVERLAYPARSE,GstTimeOverlayParse))
#define GST_TIMEOVERLAYPARSE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIMEOVERLAYPARSE,GstTimeOverlayParseClass))
#define GST_IS_TIMEOVERLAYPARSE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIMEOVERLAYPARSE))
#define GST_IS_TIMEOVERLAYPARSE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIMEOVERLAYPARSE))

typedef struct _GstTimeOverlayParse GstTimeOverlayParse;
typedef struct _GstTimeOverlayParseClass GstTimeOverlayParseClass;

struct _GstTimeOverlayParse
{
  GstVideoFilter base_timeoverlayparse;

  fec_scheme fec_scheme;
  fec decoder;
  unsigned int rows;
  unsigned int fec_n;
  unsigned int fec_k;
  unsigned char *msg_enc;
  unsigned char *msg_dec;
};

struct _GstTimeOverlayParseClass
{
  GstVideoFilterClass base_timeoverlayparse_class;
};

GType gst_timeoverlayparse_get_type (void);

G_END_DECLS

#endif
