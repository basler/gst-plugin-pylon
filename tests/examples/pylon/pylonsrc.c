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

/*
 * Example showing usage of the pylonsrc element
 */

#include <gst/gst.h>
#ifdef G_OS_UNIX
#  include <glib-unix.h>
#endif
#include <stdlib.h>

#define PYLONSRC_NAME "src"
#define OVERLAY_NAME "overlay"

typedef struct _Context Context;
struct _Context {
  GMainLoop *loop;
  GstElement *pylonsrc;
  GstElement *overlay;
};

#ifdef G_OS_UNIX
static gboolean sig_handler(Context *ctx) {
  g_return_val_if_fail(ctx, FALSE);
  g_return_val_if_fail(ctx->loop, FALSE);

  g_print("Interrupt caught, exiting...");
  g_main_loop_quit(ctx->loop);

  return TRUE;
}
#endif

static gboolean toggle_pattern(Context *ctx) {
  gint pattern = 0;
  const gchar *name_list[] = {"Off", "Testimage1", "Testimage2"};
  const gchar *name = NULL;
  GstChildProxy *cp = NULL;

  g_return_val_if_fail(ctx, FALSE);
  g_return_val_if_fail(ctx->pylonsrc, FALSE);
  g_return_val_if_fail(ctx->overlay, FALSE);

  cp = GST_CHILD_PROXY(ctx->pylonsrc);

  gst_child_proxy_get(cp, "cam::TestImageSelector", &pattern, NULL);

  /* Toggle test image pattern */
  pattern = (pattern + 1) % 3;
  name = name_list[pattern];

  gst_child_proxy_set(cp, "cam::TestImageSelector", pattern, NULL);
  g_object_set(ctx->overlay, "text", name, NULL);

  return TRUE;
}

static void print_error(GstMessage *msg, GError *error, gchar *dbg,
                        const gchar *tag) {
  g_return_if_fail(msg);
  g_return_if_fail(error);
  g_return_if_fail(tag);

  g_printerr("%s from element %s: %s\n", tag, GST_OBJECT_NAME(msg->src),
             error->message);
  g_printerr("Debugging info: %s\n", (dbg) ? dbg : "none");
  g_error_free(error);
  g_free(dbg);
}

static gboolean bus_callback(GstBus *bus, GstMessage *msg, Context *ctx) {
  GError *err = NULL;
  gchar *dbg_info = NULL;

  g_return_val_if_fail(ctx, FALSE);
  g_return_val_if_fail(ctx->loop, FALSE);

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &dbg_info);
      print_error(msg, err, dbg_info, "ERROR");
      /* Treat errors as fatal and tear down the app */
      g_main_loop_quit(ctx->loop);
      break;
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning(msg, &err, &dbg_info);
      print_error(msg, err, dbg_info, "WARNING");
      break;
    default:
      break;
  }

  return TRUE;
}

int main(int argc, char **argv) {
  Context ctx = {0};
  GstElement *pipe = NULL;
  GstBus *bus = NULL;
  guint bus_watch = 0;
  GError *error = NULL;
  gint ret = EXIT_FAILURE;
  const gchar *desc =
      "pylonsrc device-serial-number=0815-0000 name=" PYLONSRC_NAME
      " ! textoverlay auto-resize=true text=Testimage2 name=" OVERLAY_NAME
      " ! queue ! videoconvert ! autovideosink";

  /* Make sure we have an emulator running */
  g_setenv("PYLON_CAMEMU", "1", TRUE);

  gst_init(&argc, &argv);

  pipe = gst_parse_launch_full(desc, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
  if (!pipe) {
    g_printerr("Unable to create pipeline: %s\n", error->message);
    g_error_free(error);
    goto out;
  }

  ctx.pylonsrc = gst_bin_get_by_name(GST_BIN(pipe), PYLONSRC_NAME);
  if (!ctx.pylonsrc) {
    g_printerr("No pylonsrc element found\n");
    goto free_pipe;
  }

  ctx.overlay = gst_bin_get_by_name(GST_BIN(pipe), OVERLAY_NAME);
  if (!ctx.overlay) {
    g_printerr("No textoverlay element found\n");
    goto free_pylonsrc;
  }

  ctx.loop = g_main_loop_new(NULL, FALSE);
#ifdef G_OS_UNIX
  g_unix_signal_add(SIGINT, (GSourceFunc)sig_handler, &ctx);
#endif

  /* Add a bus listener to receive errors, warnings and other notifications
   * from the pipeline
   */
  bus = gst_pipeline_get_bus(GST_PIPELINE(pipe));
  bus_watch = gst_bus_add_watch(bus, (GstBusFunc)bus_callback, &ctx);
  gst_object_unref(bus);

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state(pipe, GST_STATE_PLAYING)) {
    g_printerr("Unable to play pipeline\n");
    goto free_overlay;
  }

  /* Add a periodic callback to change a camera property */
  g_timeout_add_seconds(1, (GSourceFunc)toggle_pattern, &ctx);

  /* Run until an interrupt is received */
  g_main_loop_run(ctx.loop);

  gst_element_set_state(pipe, GST_STATE_NULL);

  g_main_loop_unref(ctx.loop);
  ret = EXIT_SUCCESS;

  g_print("bye!\n");

free_overlay:
  gst_object_unref(ctx.overlay);

free_pylonsrc:
  gst_object_unref(ctx.pylonsrc);

free_pipe:
  g_source_remove(bus_watch);
  gst_object_unref(pipe);

out:
  gst_deinit();

  return ret;
}
