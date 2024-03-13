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
 * SECTION:element-gsttimestampoverlay
 *
 * The timestampoverlay element records various timestamps onto the video.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! timestampoverlay ! FIXME ! fakesink
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
#include "gsttimestampoverlay.h"

#include <string.h>
#include <sys/time.h>
#include <inttypes.h>

GST_DEBUG_CATEGORY_STATIC (gst_timestampoverlay_debug_category);
#define GST_CAT_DEFAULT gst_timestampoverlay_debug_category

/* prototypes */
static void gst_timestampoverlay_dispose (GObject *object);
static gboolean gst_timestampoverlay_src_event (GstBaseTransform *
    basetransform, GstEvent * event);
static GstFlowReturn gst_timestampoverlay_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static gboolean gst_timestampoverlay_set_clock (GstElement * element,
    GstClock * clock);

enum
{
  PROP_0,
  PROP_FEC_SCHEME
};

static void
gst_timestampoverlay_set_fec_scheme (GstTimeStampOverlay *overlay,
                                     fec_scheme fs)
{
  overlay->fec_scheme = fs;

  if (overlay->encoder) {
      fec_destroy(overlay->encoder);
      overlay->encoder = NULL;
  }
  free(overlay->msg_org);
  free(overlay->msg_enc);
  overlay->msg_org = NULL;
  overlay->msg_enc = NULL;

  overlay->encoder =  fec_create(fs, NULL);

  // decoded message length (bytes)
  unsigned int n = 8;
  // compute encoded message length
  unsigned int k = fec_get_enc_msg_length(fs, n);

  overlay->fec_n = n;
  overlay->fec_k = k;
  overlay->msg_org = malloc(sizeof(char)* n);
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
gst_timestampoverlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTimeStampOverlay *overlay = GST_TIMESTAMPOVERLAY (object);

  switch (prop_id) {
  case PROP_FEC_SCHEME:
    gst_timestampoverlay_set_fec_scheme (overlay, g_value_get_enum (value));
    break;
  default:
    break;
  }
}

static void
gst_timestampoverlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTimeStampOverlay *overlay = GST_TIMESTAMPOVERLAY (object);

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
    GST_VIDEO_CAPS_MAKE("{RGB, BGR, BGRx, xBGR, RGB, RGBx, xRGB, RGB15, RGB16}")

/* FIXME: add/remove formats you can handle */
#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{RGB, BGR, BGRx, xBGR, RGB, RGBx, xRGB, RGB15, RGB16}")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstTimeStampOverlay, gst_timestampoverlay, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_timestampoverlay_debug_category, "timestampoverlay", 0,
  "debug category for timestampoverlay element"));

static void
gst_timestampoverlay_class_init (GstTimeStampOverlayClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (gstelement_class,
      "Timestampoverlay", "Generic", "Draws the various timestamps on the "
      "video so they can be read off the video afterwards",
      "William Manley <will@williammanley.net>");

  /* define virtual function pointers */
  gobject_class->set_property = gst_timestampoverlay_set_property;
  gobject_class->get_property = gst_timestampoverlay_get_property;

  /* define properties */
  g_object_class_install_property (gobject_class, PROP_FEC_SCHEME,
    g_param_spec_enum ("fec-scheme", "Foward Error Correction Scheme",
                       "FEC Scheme to use",
                       GST_TYPE_FEC_SCHEME, LIQUID_FEC_NONE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_timestampoverlay_dispose);
  gstelement_class->set_clock = GST_DEBUG_FUNCPTR (gst_timestampoverlay_set_clock);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_timestampoverlay_src_event);
  video_filter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (gst_timestampoverlay_transform_frame_ip);
}

static void
gst_timestampoverlay_init (GstTimeStampOverlay *overlay)
{
  GST_OBJECT_FLAG_SET (overlay, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
  overlay->frame_id = 0;

  overlay->latency = GST_CLOCK_TIME_NONE;
  overlay->realtime_clock = g_object_new (GST_TYPE_SYSTEM_CLOCK,
      "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
  GST_OBJECT_FLAG_SET (overlay->realtime_clock,
      GST_CLOCK_FLAG_CAN_SET_MASTER);

  overlay->encoder = NULL;
  overlay->msg_org = NULL;
  overlay->msg_enc = NULL;
  gst_timestampoverlay_set_fec_scheme (overlay, LIQUID_FEC_NONE);
}

static void
gst_timestampoverlay_dispose (GObject *object)
{
  GstTimeStampOverlay *timeoverlay = GST_TIMESTAMPOVERLAY (object);
  g_clear_object (&timeoverlay->realtime_clock);

  if (timeoverlay->encoder) {
    fec_destroy(timeoverlay->encoder);
  }
  free(timeoverlay->msg_org);
  free(timeoverlay->msg_enc);
  timeoverlay->msg_org = NULL;
  timeoverlay->msg_enc = NULL;
}

static gboolean
gst_timestampoverlay_src_event (GstBaseTransform * basetransform, GstEvent * event)
{
  GstTimeStampOverlay *timeoverlay = GST_TIMESTAMPOVERLAY (basetransform);

  if (GST_EVENT_TYPE (event) == GST_EVENT_LATENCY) {
    GstClockTime latency = GST_CLOCK_TIME_NONE;
    gst_event_parse_latency (event, &latency);
    GST_OBJECT_LOCK (timeoverlay);
    timeoverlay->latency = latency;
    GST_OBJECT_UNLOCK (timeoverlay);
  }

  /* Chain up */
  return
      GST_BASE_TRANSFORM_CLASS (gst_timestampoverlay_parent_class)->src_event
      (basetransform, event);
}

static gboolean
gst_timestampoverlay_set_clock (GstElement * element, GstClock * clock)
{
  GstTimeStampOverlay *timestampoverlay = GST_TIMESTAMPOVERLAY (element);

  GST_DEBUG_OBJECT (timestampoverlay, "set_clock (%" GST_PTR_FORMAT ")", clock);

  if (gst_clock_set_master (timestampoverlay->realtime_clock, clock)) {
    if (clock) {
      /* gst_clock_set_master is asynchronous and may take some time to sync.
       * To give it a helping hand we'll initialise it here so we don't send
       * through spurious timings with the first buffer. */
      gst_clock_set_calibration (timestampoverlay->realtime_clock,
          gst_clock_get_internal_time (timestampoverlay->realtime_clock),
          gst_clock_get_time (clock), 1, 1);
    }
  } else {
    GST_WARNING_OBJECT (element, "Failed to slave internal REALTIME clock %"
        GST_PTR_FORMAT " to master clock %" GST_PTR_FORMAT,
        timestampoverlay->realtime_clock, clock);
  }

  return GST_ELEMENT_CLASS (gst_timestampoverlay_parent_class)->set_clock (element,
      clock);
}

static void
draw_timestamp(int lineoffset, GstClockTime timestamp, unsigned char* buf,
    size_t stride, int pxsize)
{
  int bit, line;
  buf += lineoffset * 8 * stride;

  for (line = 0; line < 8; line++) {
    for (bit = 0; bit < 64; bit++) {
      char color = ((timestamp >> (63 - bit)) & 1) * 255;
      memset(buf + bit * pxsize * 8, color, pxsize * 8);
    }
    buf += stride;
  }
}

static GstFlowReturn
gst_timestampoverlay_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  GstTimeStampOverlay *overlay = GST_TIMESTAMPOVERLAY (filter);

  GST_DEBUG_OBJECT (overlay, "transform_frame_ip");

  struct timespec systime_st;
  GstClockTime systime0;
  uint64_t systime;
  GstSegment *segment = &GST_BASE_TRANSFORM (overlay)->segment;
  unsigned char * imgdata;


  if (frame->info.stride[0] < (8 * frame->info.finfo->pixel_stride[0] * 64)) {
    GST_WARNING_OBJECT (filter, "Can't draw timestamps: video-frame is to narrow");
    return GST_FLOW_OK;
  }

  clock_gettime(CLOCK_REALTIME, &systime_st);
  systime0 = (GstClockTime)systime_st.tv_sec * 1000000000 + systime_st.tv_nsec;
  systime = (uint64_t)systime0 & 0xFFffFFffFF000000ULL;

  overlay->frame_id++;
  systime |= overlay->frame_id;
  GST_INFO_OBJECT (filter, "systime: %" PRIx64 ", frame_id: %" PRIx64,
                   systime, overlay->frame_id);


  if (overlay->fec_k > 0) {
    fec_encode(overlay->encoder, overlay->fec_n,
               (unsigned char*)&systime, overlay->msg_enc);
  } else {
    // scheme == unknown, or some other corner case.
    memcpy(overlay->msg_enc, &systime, sizeof(systime));
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
    uint64_t part = msg[r];
    draw_timestamp (r,
                    part,
                    imgdata,
                    frame->info.stride[0],
                    frame->info.finfo->pixel_stride[0]);
  }

  return GST_FLOW_OK;
}
