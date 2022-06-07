/* Copyright (C) 2022 Basler AG
 *
 * Created by RidgeRun, LLC <support@ridgerun.com>
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

#include "gstpylonintrospection.h"

/* prototypes */
static void gst_pylon_make_spec_int(GenApi::INode *node, GParamSpec **spec);
static void gst_pylon_make_spec_bool(GenApi::INode *node, GParamSpec **spec);
static void gst_pylon_make_spec_float(GenApi::INode *node, GParamSpec **spec);
static void gst_pylon_make_spec_str(GenApi::INode *node, GParamSpec **spec);
static void gst_pylon_make_spec_enum(GenApi::INode *node, GParamSpec **spec);
static GParamFlags gst_pylon_query_access(GenApi::INode *node);

static GParamFlags gst_pylon_query_access(GenApi::INode *node) {
  Pylon::CParameter param(node);
  gint flags = 0;

  if (param.IsWritable() && param.IsReadable()) {
    flags = G_PARAM_READWRITE;
  } else if (param.IsWritable() && !param.IsReadable()) {
    flags = G_PARAM_WRITABLE;
  } else if (!param.IsWritable() && param.IsReadable()) {
    flags = G_PARAM_READABLE;
  }

  GenICam::gcstring value;
  GenICam::gcstring attribute;
  if (node->GetProperty("pIsLocked", value, attribute)) {
    flags = flags | GST_PARAM_MUTABLE_READY;
  } else {
    flags = flags | GST_PARAM_MUTABLE_PLAYING;
  }

  return static_cast<GParamFlags>(flags);
}

static void gst_pylon_make_spec_int(GenApi::INode *node, GParamSpec **spec) {
  g_return_if_fail(node);
  g_return_if_fail(spec);

  Pylon::CIntegerParameter param(node);

  *spec = g_param_spec_int(node->GetName(), node->GetDisplayName(),
                           node->GetDescription(), param.GetMin(),
                           param.GetMax(), param.GetValueOrDefault(-1),
                           gst_pylon_query_access(node));
}

static void gst_pylon_make_spec_bool(GenApi::INode *node, GParamSpec **spec) {
  g_return_if_fail(node);
  g_return_if_fail(spec);

  Pylon::CBooleanParameter param(node);

  *spec = g_param_spec_boolean(
      node->GetName(), node->GetDisplayName(), node->GetDescription(),
      param.GetValueOrDefault(FALSE), gst_pylon_query_access(node));
}

static void gst_pylon_make_spec_float(GenApi::INode *node, GParamSpec **spec) {
  g_return_if_fail(node);
  g_return_if_fail(spec);

  Pylon::CFloatParameter param(node);

  *spec =
      g_param_spec_float(node->GetName(), node->GetDisplayName(),
                         node->GetDescription(), param.GetMin(), param.GetMax(),
                         param.GetValue(), gst_pylon_query_access(node));
}

static void gst_pylon_make_spec_str(GenApi::INode *node, GParamSpec **spec) {
  g_return_if_fail(node);
  g_return_if_fail(spec);

  Pylon::CStringParameter param(node);

  *spec = g_param_spec_string(node->GetName(), node->GetDisplayName(),
                              node->GetDescription(), param.GetValue(),
                              gst_pylon_query_access(node));
}

static void gst_pylon_make_spec_enum(GenApi::INode *node, GParamSpec **spec) {
  g_return_if_fail(node);
  g_return_if_fail(spec);

  Pylon::CEnumParameter param(node);

  *spec = g_param_spec_enum(node->GetName(), node->GetDisplayName(),
                            node->GetDescription(), G_TYPE_INT,
                            param.GetIntValue(), gst_pylon_query_access(node));
}

GParamSpec *GstPylonParamFactory::make_param(GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;

  if (GenApi::intfIInteger == node->GetPrincipalInterfaceType()) {
    gst_pylon_make_spec_int(node, &spec);

  } else if (GenApi::intfIBoolean == node->GetPrincipalInterfaceType()) {
    gst_pylon_make_spec_bool(node, &spec);

  } else if (GenApi::intfIFloat == node->GetPrincipalInterfaceType()) {
    gst_pylon_make_spec_float(node, &spec);

  } else if (GenApi::intfIString == node->GetPrincipalInterfaceType()) {
    gst_pylon_make_spec_str(node, &spec);

  } else if (GenApi::intfIEnumeration == node->GetPrincipalInterfaceType()) {
    gst_pylon_make_spec_enum(node, &spec);

  } else {
    throw Pylon::GenericException("Cannot set param spec, invalid node",
                                  __FILE__, __LINE__);
  }

  return spec;
}
