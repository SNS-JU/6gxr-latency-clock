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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gsttimeoverlayparse
 *
 * The timeoverlayparse element records various timestamps onto the video.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! timeoverlayparse ! timeoverlayparse ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gsttimeoverlayparse.h"

#include <string.h>
#include <sys/time.h>
#include <inttypes.h>

#include "gsttimestampcommon.h"

GST_DEBUG_CATEGORY_STATIC (gst_timeoverlayparse_debug_category);
#define GST_CAT_DEFAULT gst_timeoverlayparse_debug_category

/* prototypes */
static GstFlowReturn gst_timeoverlayparse_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_FEC_SCHEME
};

static void
gst_timeoverlayparse_set_fec_scheme (GstTimeOverlayParse *overlay,
                                     fec_scheme fs)
{
  overlay->fec_scheme = fs;

  if (overlay->decoder) {
      fec_destroy(overlay->decoder);
      overlay->decoder = NULL;
  }
  free(overlay->msg_dec);
  free(overlay->msg_enc);
  overlay->msg_dec = NULL;
  overlay->msg_enc = NULL;

  overlay->decoder =  fec_create(fs, NULL);

  // decoded message length (bytes)
  unsigned int n = 8;
  // compute encoded message length
  unsigned int k = fec_get_enc_msg_length(fs, n);

  overlay->fec_n = n;
  overlay->fec_k = k;
  overlay->msg_dec = malloc(sizeof(char)* n);
  overlay->msg_enc = malloc(sizeof(char) * (8 + k));
  memset(overlay->msg_enc, 0, sizeof(char) * (8 + k));
  overlay->rows = k / 8;
  if (overlay->rows * 8 != n) {
    overlay->rows ++;
  }
  GST_INFO_OBJECT (overlay, "set_property: fec_scheme n:%u k:%u rows:%u",
                   n, k, overlay->rows);
}

static void
gst_timeoverlayparse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTimeOverlayParse *overlay = GST_TIMEOVERLAYPARSE (object);
  GST_INFO_OBJECT (overlay, "set_property");

  switch (prop_id) {
  case PROP_FEC_SCHEME:
    gst_timeoverlayparse_set_fec_scheme (overlay, g_value_get_enum (value));
    break;
  default:
    break;
  }
}

static void
gst_timeoverlayparse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTimeOverlayParse *overlay = GST_TIMEOVERLAYPARSE (object);

  switch (prop_id) {
  case PROP_FEC_SCHEME:
    g_value_set_enum (value, overlay->fec_scheme);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

/* pad templates */

/* FIXME: add/remove formats you can handle */
#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{RGB, xRGB, BGR, BGRx}")

/* FIXME: add/remove formats you can handle */
#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{RGB, xRGB, BGR, BGRx}")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstTimeOverlayParse, gst_timeoverlayparse, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_timeoverlayparse_debug_category, "timeoverlayparse", 0,
  "debug category for timeoverlayparse element"));

static void
gst_timeoverlayparse_class_init (GstTimeOverlayParseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "TimeOverlayParse", "Generic", "Reads the various timestamps from the "
      "video written by timestampoverlay",
      "William Manley <will@williammanley.net>");

  /* define virtual function pointers */
  gobject_class->set_property = gst_timeoverlayparse_set_property;
  gobject_class->get_property = gst_timeoverlayparse_get_property;

  /* define properties */
  g_object_class_install_property (gobject_class, PROP_FEC_SCHEME,
    g_param_spec_enum ("fec-scheme", "Foward Error Correction Scheme",
                       "FEC Scheme to use",
                       GST_TYPE_FEC_SCHEME, LIQUID_FEC_NONE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  video_filter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (gst_timeoverlayparse_transform_frame_ip);
}

static void
gst_timeoverlayparse_init (GstTimeOverlayParse *obj)
{
  obj->decoder = NULL;
  obj->msg_enc = NULL;
  obj->msg_dec = NULL;
  gst_timeoverlayparse_set_fec_scheme (obj, LIQUID_FEC_NONE);
}

static GstClockTime
read_timestamp(int lineoffset, unsigned char* buf, size_t stride, int pxsize)
{
  int bit;
  GstClockTime timestamp = 0;

  buf += (lineoffset * 8 + 4) * stride;

  for (bit = 0; bit < 64; bit++) {
    char color = buf[bit * pxsize * 8 + 4];
    timestamp |= (color & 0x80) ?  (guint64) 1 << (63 - bit) : 0;
  }

  return timestamp;
}

static GstFlowReturn
gst_timeoverlayparse_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  struct timespec systime_st;
  clock_gettime(CLOCK_REALTIME, &systime_st);
  GstClockTime systime = (GstClockTime)systime_st.tv_sec * 1000000000 + systime_st.tv_nsec;

  GstTimeOverlayParse *overlay = GST_TIMEOVERLAYPARSE (filter);
  unsigned char * imgdata;

  GST_DEBUG_OBJECT (overlay, "transform_frame_ip");

  GstClockTimeDiff latency;
  GstSegment *segment = &GST_BASE_TRANSFORM (overlay)->segment;

  if (frame->info.stride[0] < (8 * frame->info.finfo->pixel_stride[0] * 64)) {
    GST_WARNING_OBJECT (filter, "Can't read timestamps: video-frame is to narrow");
    return GST_FLOW_OK;
  }

  imgdata = frame->data[0];

  /* Centre Vertically: */
  unsigned int rows = overlay->rows;
  imgdata += (frame->info.height - rows * 8) * frame->info.stride[0] / 2;

  /* Centre Horizontally: */
  imgdata += (frame->info.width - 64 * 8) * frame->info.finfo->pixel_stride[0]
      / 2;

  uint64_t *msg = (uint64_t*)overlay->msg_enc;
  for (int r = 0; r < overlay->rows; r++) {
    uint64_t *part = &msg[r];
    *part = read_timestamp (r,
                            imgdata,
                            frame->info.stride[0],
                            frame->info.finfo->pixel_stride[0]);
  }
  uint64_t info;
  if (overlay->fec_k > 0) {
    fec_decode(overlay->decoder, overlay->fec_n,
               overlay->msg_enc, (unsigned char*)&info);
  } else {
    memcpy(overlay->msg_enc, &info, sizeof(info));
  }

  uint64_t frame_id = 0xffFFffULL & info;
  GstClockTime remote_time = (GstClockTime)( info & 0xFFffFFffFF000000ULL );
  latency = systime - remote_time;

  GST_INFO_OBJECT (filter, "Systime: %ld; Latency: %ld; Frame-id: %lu",
      GST_TIME_AS_NSECONDS(systime),
      GST_TIME_AS_NSECONDS(latency),
      frame_id);

  return GST_FLOW_OK;
}
