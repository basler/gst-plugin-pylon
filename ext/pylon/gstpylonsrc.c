/* Copyright (C) 2022 Basler AG
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION:element-gstpylonsrc
 *
 * The pylonsrc element captures images from Basler cameras.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v pylonsrc ! videoconvert ! autovideosink
 * ]|
 * Capture images from a Basler camera and display them.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpylonsrc.h"

#include "gstpylon.h"

#include <gst/video/video.h>

#define GST_CAT_DEFAULT gst_pylon_src_debug_category

struct _GstPylonSrc
{
  GstPushSrc base_pylonsrc;
  GstPylon *pylon;
  guint64 offset;
  GstClockTime duration;
  GstVideoInfo video_info;

  gchar *device_user_name;
  gchar *device_serial_number;
  gint device_index;
  gchar *user_set;
  gchar *pfs_location;
  GObject *cam;
  GObject *stream;
};

/* prototypes */


static void
gst_pylon_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_pylon_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_pylon_src_finalize (GObject * object);

static GstCaps *gst_pylon_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static GstCaps *gst_pylon_src_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_pylon_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_pylon_src_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_pylon_src_start (GstBaseSrc * src);
static gboolean gst_pylon_src_stop (GstBaseSrc * src);
static gboolean gst_pylon_src_unlock (GstBaseSrc * src);
static gboolean gst_pylon_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_pylon_src_query (GstBaseSrc * src, GstQuery * query);
static void gst_plyon_src_add_metadata (GstPylonSrc * self, GstBuffer * buf);
static GstFlowReturn gst_pylon_src_create (GstPushSrc * src, GstBuffer ** buf);

static void gst_pylon_src_child_proxy_init (GstChildProxyInterface * iface);

enum
{
  PROP_0,
  PROP_DEVICE_USER_NAME,
  PROP_DEVICE_SERIAL_NUMBER,
  PROP_DEVICE_INDEX,
  PROP_USER_SET,
  PROP_PFS_LOCATION,
  PROP_CAM,
  PROP_STREAM
};

#define PROP_DEVICE_USER_NAME_DEFAULT NULL
#define PROP_DEVICE_SERIAL_NUMBER_DEFAULT NULL
#define PROP_DEVICE_INDEX_DEFAULT -1
#define PROP_DEVICE_INDEX_MIN -1
#define PROP_DEVICE_INDEX_MAX G_MAXINT32
#define PROP_USER_SET_DEFAULT NULL
#define PROP_PFS_LOCATION_DEFAULT NULL
#define PROP_CAM_DEFAULT NULL
#define PROP_STREAM_DEFAULT NULL

/* pad templates */

static GstStaticPadTemplate gst_pylon_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (" {GRAY8, RGB, BGR, YUY2, UYVY} "))
    );


/* class initialization */
GST_DEBUG_CATEGORY (gst_pylon_src_debug_category);
G_DEFINE_TYPE_WITH_CODE (GstPylonSrc, gst_pylon_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_pylon_src_debug_category, "pylonsrc", 0,
        "debug category for pylonsrc element");
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY, gst_pylon_src_child_proxy_init)
    );

static void
gst_pylon_src_class_init (GstPylonSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);
  gchar *cam_params = NULL;
  gchar *cam_blurb = NULL;
  gchar *stream_params = NULL;
  gchar *stream_blurb = NULL;
  const gchar *cam_prolog = NULL;
  const gchar *stream_prolog = NULL;

  gst_pylon_initialize ();

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_pylon_src_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Basler/Pylon source element", "Source/Video/Hardware",
      "Source element for Basler cameras",
      "Basler AG <support.europe@baslerweb.com>");

  gobject_class->set_property = gst_pylon_src_set_property;
  gobject_class->get_property = gst_pylon_src_get_property;
  gobject_class->finalize = gst_pylon_src_finalize;

  g_object_class_install_property (gobject_class, PROP_DEVICE_USER_NAME,
      g_param_spec_string ("device-user-name", "Device user defined name",
          "The user-defined name of the device to use. May be combined"
          "with other device selection properties to reduce the search.",
          PROP_DEVICE_USER_NAME_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_DEVICE_SERIAL_NUMBER,
      g_param_spec_string ("device-serial-number", "Device serial number",
          "The serial number of the device to use. May be combined with "
          "other device selection properties to reduce the search.",
          PROP_DEVICE_SERIAL_NUMBER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "The index of the device to use.This index applies to the "
          "resulting device list after applying the other device selection "
          "properties. The index is mandatory if multiple devices match "
          "the given search criteria.", PROP_DEVICE_INDEX_MIN,
          PROP_DEVICE_INDEX_MAX, PROP_DEVICE_INDEX_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_USER_SET,
      g_param_spec_string ("user-set", "Device user configuration set",
          "The user-defined configuration set to use. Leaving this property "
          "unset, or using 'Auto' result in selecting the "
          "power-on default camera configuration.",
          PROP_USER_SET_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_PFS_LOCATION,
      g_param_spec_string ("pfs-location", "PFS file location",
          "The filepath to the PFS file from which to load the device configuration. "
          "Setting this property will override the user set property if also set.",
          PROP_PFS_LOCATION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  cam_params = gst_pylon_camera_get_string_properties ();
  stream_params = gst_pylon_stream_grabber_get_string_properties ();

  if (NULL == cam_params) {
    cam_prolog = "No valid cameras where found connected to the system.";
    stream_prolog = cam_prolog;
    cam_params = g_strdup ("");
    stream_params = g_strdup ("");
  } else {
    cam_prolog = "The following list details the properties for each camera.\n";
    stream_prolog =
        "The following list details the properties for each stream grabber.\n";
  }

  cam_blurb = g_strdup_printf ("The camera to use.\n"
      "\t\t\tAccording to the selected camera "
      "different properties will be available.\n "
      "\t\t\tThese properties can be accessed using the "
      "\"cam::<property>\" syntax.\n" "\t\t\t%s%s", cam_prolog, cam_params);

  g_object_class_install_property (gobject_class, PROP_CAM,
      g_param_spec_object ("cam", "Camera", cam_blurb, G_TYPE_OBJECT,
          G_PARAM_READABLE));

  stream_blurb = g_strdup_printf ("The stream grabber to use.\n"
      "\t\t\tAccording to the selected stream grabber "
      "different properties will be available.\n "
      "\t\t\tThese properties can be accessed using the "
      "\"stream::<property>\" syntax.\n" "\t\t\t%s%s", stream_prolog,
      stream_params);

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_object ("stream", "Stream Grabber", stream_blurb,
          G_TYPE_OBJECT, G_PARAM_READABLE));

  g_free (cam_params);
  g_free (stream_params);

  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_pylon_src_get_caps);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_pylon_src_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_pylon_src_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_pylon_src_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_pylon_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_pylon_src_stop);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_pylon_src_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_pylon_src_unlock_stop);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_pylon_src_query);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_pylon_src_create);
}

static void
gst_pylon_src_init (GstPylonSrc * self)
{
  GstBaseSrc *base = GST_BASE_SRC (self);

  self->pylon = NULL;
  self->offset = G_GUINT64_CONSTANT (0);
  self->duration = GST_CLOCK_TIME_NONE;
  self->device_user_name = PROP_DEVICE_USER_NAME_DEFAULT;
  self->device_serial_number = PROP_DEVICE_SERIAL_NUMBER_DEFAULT;
  self->device_index = PROP_DEVICE_INDEX_DEFAULT;
  self->user_set = PROP_USER_SET_DEFAULT;
  self->pfs_location = PROP_PFS_LOCATION_DEFAULT;
  self->cam = PROP_CAM_DEFAULT;
  self->stream = PROP_STREAM_DEFAULT;
  gst_video_info_init (&self->video_info);

  gst_base_src_set_live (base, TRUE);
  gst_base_src_set_format (base, GST_FORMAT_TIME);
}

static void
gst_pylon_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);

  switch (property_id) {
    case PROP_DEVICE_USER_NAME:
      g_free (self->device_user_name);
      self->device_user_name = g_value_dup_string (value);
      break;
    case PROP_DEVICE_SERIAL_NUMBER:
      g_free (self->device_serial_number);
      self->device_serial_number = g_value_dup_string (value);
      break;
    case PROP_DEVICE_INDEX:
      self->device_index = g_value_get_int (value);
      break;
    case PROP_USER_SET:
      g_free (self->user_set);
      self->user_set = g_value_dup_string (value);
      break;
    case PROP_PFS_LOCATION:
      g_free (self->pfs_location);
      self->pfs_location = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_pylon_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);

  switch (property_id) {
    case PROP_DEVICE_USER_NAME:
      g_value_set_string (value, self->device_user_name);
      break;
    case PROP_DEVICE_SERIAL_NUMBER:
      g_value_set_string (value, self->device_serial_number);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, self->device_index);
      break;
    case PROP_USER_SET:
      g_value_set_string (value, self->user_set);
      break;
    case PROP_PFS_LOCATION:
      g_value_set_string (value, self->pfs_location);
      break;
    case PROP_CAM:
      g_value_set_object (value, self->cam);
      break;
    case PROP_STREAM:
      g_value_set_object (value, self->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_pylon_src_finalize (GObject * object)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "finalize");

  g_free (self->device_user_name);
  self->device_user_name = NULL;

  g_free (self->device_serial_number);
  self->device_serial_number = NULL;

  g_free (self->user_set);
  self->user_set = NULL;

  if (self->cam) {
    g_object_unref (self->cam);
    self->cam = NULL;
  }

  if (self->stream) {
    g_object_unref (self->stream);
    self->stream = NULL;
  }

  G_OBJECT_CLASS (gst_pylon_src_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_pylon_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GstCaps *outcaps = NULL;
  GError *error = NULL;

  if (!self->pylon) {
    outcaps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (self));
    GST_INFO_OBJECT (self,
        "Camera not open yet, returning src template caps %" GST_PTR_FORMAT,
        outcaps);
    goto out;
  }

  outcaps = gst_pylon_query_configuration (self->pylon, &error);

  if (outcaps == NULL && error) {
    goto log_gst_error;
  }

  GST_DEBUG_OBJECT (self, "Camera returned caps %" GST_PTR_FORMAT, outcaps);

  if (filter) {
    GstCaps *tmp = outcaps;

    GST_DEBUG_OBJECT (self, "Filtering with %" GST_PTR_FORMAT, filter);

    outcaps = gst_caps_intersect (outcaps, filter);
    gst_caps_unref (tmp);
  }

  GST_INFO_OBJECT (self, "Returning caps %" GST_PTR_FORMAT, outcaps);

  goto out;

log_gst_error:
  GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
      ("Failed to get caps."), ("%s", error->message));
  g_error_free (error);

out:
  return outcaps;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_pylon_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GstCaps *outcaps = NULL;
  GstStructure *st = NULL;
  static const gint preferred_width = 1920;
  static const gint preferred_height = 1080;
  static const gint preferred_framerate_num = 30;
  static const gint preferred_framerate_den = 1;


  GST_DEBUG_OBJECT (self, "Fixating caps %" GST_PTR_FORMAT, caps);

  if (gst_caps_is_fixed (caps)) {
    GST_DEBUG_OBJECT (self, "Caps are already fixed");
    return caps;
  }

  outcaps = gst_caps_new_empty ();
  st = gst_structure_copy (gst_caps_get_structure (caps, 0));
  gst_caps_unref (caps);

  gst_structure_fixate_field_nearest_int (st, "width", preferred_width);
  gst_structure_fixate_field_nearest_int (st, "height", preferred_height);
  gst_structure_fixate_field_nearest_fraction (st, "framerate",
      preferred_framerate_num, preferred_framerate_den);

  gst_caps_append_structure (outcaps, st);

  /* Fixate the remainder of the fields */
  outcaps = gst_caps_fixate (outcaps);

  GST_INFO_OBJECT (self, "Fixated caps to %" GST_PTR_FORMAT, outcaps);

  return outcaps;
}

/* notify the subclass of new caps */
static gboolean
gst_pylon_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GstStructure *st = NULL;
  gint numerator = 0;
  gint denominator = 0;
  GError *error = NULL;
  gboolean ret = FALSE;
  const gchar *action = NULL;

  GST_INFO_OBJECT (self, "Setting new caps: %" GST_PTR_FORMAT, caps);

  st = gst_caps_get_structure (caps, 0);
  gst_structure_get_fraction (st, "framerate", &numerator, &denominator);

  GST_OBJECT_LOCK (self);
  if (numerator != 0) {
    self->duration = gst_util_uint64_scale (GST_SECOND, denominator, numerator);
  } else {
    self->duration = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (self);

  ret = gst_pylon_stop (self->pylon, &error);
  if (FALSE == ret && error) {
    action = "stop";
    goto error;
  }

  ret = gst_pylon_set_configuration (self->pylon, caps, &error);
  if (FALSE == ret && error) {
    action = "configure";
    goto error;
  }

  ret = gst_pylon_start (self->pylon, &error);
  if (FALSE == ret && error) {
    action = "start";
    goto error;
  }

  ret = gst_video_info_from_caps (&self->video_info, caps);

  goto out;

error:
  GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
      ("Failed to %s camera.", action), ("%s", error->message));
  g_error_free (error);

out:
  return ret;
}

/* setup allocation query */
static gboolean
gst_pylon_src_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "decide_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_pylon_src_start (GstBaseSrc * src)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GError *error = NULL;
  gboolean ret = TRUE;
  gboolean using_pfs = FALSE;

  if (self->pylon) {
    return TRUE;
  }

  GST_OBJECT_LOCK (self);
  GST_INFO_OBJECT (self,
      "Attempting to create camera device with the following configuration:"
      "\n\tname: %s\n\tserial number: %s\n\tindex: %d\n\tuser set: %s \n\tPFS filepath: %s."
      "If defined, the PFS file will override the user set configuration.",
      self->device_user_name, self->device_serial_number, self->device_index,
      self->user_set, self->pfs_location);

  self->pylon =
      gst_pylon_new (self->device_user_name, self->device_serial_number,
      self->device_index, &error);
  GST_OBJECT_UNLOCK (self);

  if (error) {
    ret = FALSE;
    goto log_gst_error;
  }

  GST_OBJECT_LOCK (self);
  ret = gst_pylon_set_user_config (self->pylon, self->user_set, &error);
  GST_OBJECT_UNLOCK (self);

  if (ret == FALSE && error) {
    goto log_gst_error;
  }

  GST_OBJECT_LOCK (self);
  if (self->pfs_location) {
    using_pfs = TRUE;
    ret = gst_pylon_set_pfs_config (self->pylon, self->pfs_location, &error);
  }
  GST_OBJECT_UNLOCK (self);

  if (using_pfs && ret == FALSE && error) {
    goto log_gst_error;
  }

  self->offset = G_GUINT64_CONSTANT (0);
  self->duration = GST_CLOCK_TIME_NONE;

  goto out;

log_gst_error:
  GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
      ("Failed to start camera."), ("%s", error->message));
  g_error_free (error);

out:
  return ret;
}

static gboolean
gst_pylon_src_stop (GstBaseSrc * src)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GError *error = NULL;
  gboolean ret = TRUE;

  GST_INFO_OBJECT (self, "Stopping camera device");

  ret = gst_pylon_stop (self->pylon, &error);

  if (ret == FALSE && error) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Failed to close camera."), ("%s", error->message));
    g_error_free (error);
  }

  gst_pylon_free (self->pylon);
  self->pylon = NULL;

  return ret;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_pylon_src_unlock (GstBaseSrc * src)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_pylon_src_unlock_stop (GstBaseSrc * src)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "unlock_stop");

  gst_pylon_interrupt_capture (self->pylon);

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_pylon_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      GstClockTime min_latency = GST_CLOCK_TIME_NONE;
      GstClockTime max_latency = GST_CLOCK_TIME_NONE;

      if (GST_CLOCK_TIME_NONE == self->duration) {
        GST_WARNING_OBJECT (src,
            "Can't report latency since framerate is not fixated yet");
        goto done;
      }

      /* FIXME: we are assuming we cannot hold more than one
         buffer. Eventually we want to have Pylon::StartGrabbing's
         MaxNumImages extend the max_latency.
       */
      max_latency = min_latency = self->duration;

      GST_DEBUG_OBJECT (self,
          "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
          GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

      gst_query_set_latency (query, TRUE, min_latency, max_latency);

      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (gst_pylon_src_parent_class)->query (src, query);
      break;
  }

done:
  return res;
}

/* add time metadata to buffer */
static void
gst_plyon_src_add_metadata (GstPylonSrc * self, GstBuffer * buf)
{
  GstClock *clock = NULL;
  GstClockTime abs_time = GST_CLOCK_TIME_NONE;
  GstClockTime base_time = GST_CLOCK_TIME_NONE;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  guint width = 0;
  guint height = 0;
  guint n_planes = 0;
  gint stride[GST_VIDEO_MAX_PLANES] = { 0 };

  g_return_if_fail (self);
  g_return_if_fail (buf);

  GST_OBJECT_LOCK (self);
  /* set duration */
  GST_BUFFER_DURATION (buf) = self->duration;

  if ((clock = GST_ELEMENT_CLOCK (self))) {
    /* we have a clock, get base time and ref clock */
    base_time = GST_ELEMENT (self)->base_time;
    gst_object_ref (clock);
  } else {
    /* no clock, can't set timestamps */
    base_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (self);

  /* sample pipeline clock */
  if (clock) {
    abs_time = gst_clock_get_time (clock);
    gst_object_unref (clock);
  } else {
    abs_time = GST_CLOCK_TIME_NONE;
  }

  timestamp = abs_time - base_time;

  GST_BUFFER_TIMESTAMP (buf) = timestamp;
  GST_BUFFER_OFFSET (buf) = self->offset;
  GST_BUFFER_OFFSET_END (buf) = self->offset + 1;

  /* add video meta data */
  format = GST_VIDEO_INFO_FORMAT (&self->video_info);
  width = GST_VIDEO_INFO_WIDTH (&self->video_info);
  height = GST_VIDEO_INFO_HEIGHT (&self->video_info);
  n_planes = GST_VIDEO_INFO_N_PLANES (&self->video_info);

  /* stride is being constructed manually since the pylon stride
   * does not match with the GstVideoInfo obtained stride. */
  for (gint p = 0; p < n_planes; p++) {
    stride[p] = width * GST_VIDEO_INFO_COMP_PSTRIDE (&self->video_info, p);
  }

  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE, format, width,
      height, n_planes, self->video_info.offset, stride);
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_pylon_src_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GError *error = NULL;
  gboolean pylon_ret = TRUE;
  GstFlowReturn ret = GST_FLOW_OK;

  pylon_ret = gst_pylon_capture (self->pylon, buf, &error);

  if (pylon_ret == FALSE) {
    ret = GST_FLOW_EOS;
    if (error) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Failed to create buffer."), ("%s", error->message));
      g_error_free (error);
      ret = GST_FLOW_ERROR;
    }
    goto done;
  }

  gst_plyon_src_add_metadata (self, *buf);
  self->offset++;

  GST_LOG_OBJECT (self, "Created buffer %" GST_PTR_FORMAT, *buf);

done:
  return ret;
}

static GObject *
gst_pylon_src_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstPylonSrc *self = GST_PYLON_SRC (child_proxy);
  GObject *obj = NULL;

  GST_DEBUG_OBJECT (self, "Looking for child \"%s\"", name);

  if (!gst_pylon_src_start (GST_BASE_SRC (self))) {
    GST_ERROR_OBJECT (self,
        "Please specify a camera before attempting to set Pylon device properties");
    return NULL;
  }

  if (!g_strcmp0 (name, "cam")) {
    GST_OBJECT_LOCK (self);
    obj = gst_pylon_get_camera (self->pylon);
    GST_OBJECT_UNLOCK (self);
  } else if (!g_strcmp0 (name, "stream")) {
    GST_OBJECT_LOCK (self);
    obj = gst_pylon_get_stream_grabber (self->pylon);
    GST_OBJECT_UNLOCK (self);
  } else {
    GST_ERROR_OBJECT (self,
        "No child named \"%s\". Use \"cam\" or \"stream\"  instead.", name);
  }

  return obj;
}

static guint
gst_pylon_src_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  /* There's only one camera and one stream grabber active at a time */
  return 2;
}

static void
gst_pylon_src_child_proxy_init (GstChildProxyInterface * iface)
{
  iface->get_child_by_name = gst_pylon_src_child_proxy_get_child_by_name;
  iface->get_children_count = gst_pylon_src_child_proxy_get_children_count;
}
