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
#include <glib-unix.h>

#define PYLONSRC_NAME "src"

static gboolean
sig_handler (GMainLoop * loop)
{
  g_return_val_if_fail (loop, FALSE);

  g_print ("Interrupt caught, exiting...\n");
  g_main_loop_quit (loop);

  return TRUE;
}

int
main (int argc, char **argv)
{
  GMainLoop *loop = NULL;
  GstElement *pipe = NULL;
  GstElement *pylonsrc = NULL;
  GError *error = NULL;
  const gchar *desc =
      "pylonsrc device-serial-number=0815-0000 name=" PYLONSRC_NAME
      " ! videoconvert ! autovideosink";
  gint ret = EXIT_FAILURE;

  /* Make sure we have an emulator running */
  g_setenv ("PYLON_CAMEMU", "1", TRUE);

  gst_init (&argc, &argv);

  pipe =
      gst_parse_launch_full (desc, NULL, GST_PARSE_FLAG_FATAL_ERRORS, &error);
  if (!pipe) {
    g_printerr ("Unable to create pipeline: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  pylonsrc = gst_bin_get_by_name (GST_BIN (pipe), PYLONSRC_NAME);
  if (!pylonsrc) {
    g_printerr ("No pylonsrc element found\n");
    goto free_pipe;
  }

  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (pipe,
          GST_STATE_PLAYING)) {
    g_printerr ("Unable to play pipeline\n");
    goto free_pylonsrc;
  }

  loop = g_main_loop_new (NULL, FALSE);
  g_unix_signal_add (SIGINT, G_SOURCE_FUNC (sig_handler), loop);

  g_main_loop_run (loop);

  gst_element_set_state (pipe, GST_STATE_NULL);

  g_main_loop_unref (loop);
  ret = EXIT_SUCCESS;

free_pylonsrc:
  gst_object_unref (pylonsrc);

free_pipe:
  gst_object_unref (pipe);

out:
  return ret;
}
