/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Author: Michael Gruner <michael.gruner@ridgerun.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpylonsrc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "pylonsrc", GST_RANK_NONE,
      GST_TYPE_PYLON_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    pylon, "Basler/Pylon plugin", plugin_init, VERSION,
    "Proprietary", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
