/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Author: Michael Gruner <michael.gruner@ridgerun.com>
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

GST_DEBUG_CATEGORY_STATIC (gst_pylon_src_debug_category);
#define GST_CAT_DEFAULT gst_pylon_src_debug_category

struct _GstPylonSrc
{
  GstPushSrc base_pylonsrc;
  GstPylon *pylon;
};

/* prototypes */


static void gst_pylon_src_set_property (GObject * object,
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
static GstFlowReturn gst_pylon_src_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_pylon_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstPylonSrc, gst_pylon_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_pylon_src_debug_category, "pylonsrc", 0,
        "debug category for pylonsrc element"));

static void
gst_pylon_src_class_init (GstPylonSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_pylon_src_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Basler/Pylon source element", "Source/Video/Hardware",
      "Source element for Basler cameras",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  gobject_class->set_property = gst_pylon_src_set_property;
  gobject_class->get_property = gst_pylon_src_get_property;
  gobject_class->finalize = gst_pylon_src_finalize;

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

  gst_pylon_initialize ();
}

static void
gst_pylon_src_init (GstPylonSrc * self)
{
}

static void
gst_pylon_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_pylon_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_pylon_src_finalize (GObject * object)
{
  GstPylonSrc *self = GST_PYLON_SRC (object);

  GST_LOG_OBJECT (self, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_pylon_src_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_pylon_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "get_caps");

  /* TODO: fixme */
  return gst_caps_new_any ();
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_pylon_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "fixate");

  /* TODO: fixme */
  return gst_caps_fixate (caps);
}

/* notify the subclass of new caps */
static gboolean
gst_pylon_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "set_caps");

  return TRUE;
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

  GST_LOG_OBJECT (self, "start");

  self->pylon = gst_pylon_new (&error);

  if (error) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Failed to create camera object. %s"), (error->message));
    g_free (error);
    ret = FALSE;
    goto out;
  }

  ret = gst_pylon_start (self->pylon, &error);

  if (ret == FALSE && error) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, ("Failed to open camera. %s"),
        (error->message));
    g_free (error);
  }

out:
  return ret;
}

static gboolean
gst_pylon_src_stop (GstBaseSrc * src)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);
  GError *error = NULL;
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "stop");

  ret = gst_pylon_stop (self->pylon, &error);

  if (ret == FALSE && error) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Failed to close camera. %s"), (error->message));
    g_free (error);
  }

  ret = gst_pylon_free (self->pylon);

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

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_pylon_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "query");

  return GST_BASE_SRC_CLASS (gst_pylon_src_parent_class)->query (src, query);
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_pylon_src_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstPylonSrc *self = GST_PYLON_SRC (src);

  GST_LOG_OBJECT (self, "create");

  GST_FIXME_OBJECT (self,
      "plug-in under development! not able to produce buffers yet!");

  return GST_FLOW_ERROR;
}
