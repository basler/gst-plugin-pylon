/*
 * Copyright (C) 2022 Basler
 * All Rights Reserved.
 *
 * Author: Michael Gruner <michael.gruner@ridgerun.com>
 */

#ifndef _GST_PYLON_SRC_H_
#define _GST_PYLON_SRC_H_

#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_PYLON_SRC gst_pylon_src_get_type ()
G_DECLARE_FINAL_TYPE (GstPylonSrc, gst_pylon_src,
    GST, PYLON_SRC, GstPushSrc)

G_END_DECLS

#endif
