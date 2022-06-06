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

GParamSpec *GstPylonParamFactory::make_param(GenApi::INode *node) {
  g_return_val_if_fail(node, NULL);

  GParamSpec *spec = NULL;
  GenICam::gcstring name = node->GetName();
  GenICam::gcstring display_name = node->GetDisplayName();
  GenICam::gcstring desc = node->GetDescription();
  GParamFlags flags = static_cast<GParamFlags>(
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY);

  if (GenApi::intfIInteger == node->GetPrincipalInterfaceType()) {
    Pylon::CIntegerParameter param(node);
    spec = g_param_spec_int(name.c_str(), display_name.c_str(), desc.c_str(),
                            param.GetMin(), param.GetMax(), param.GetValue(),
                            flags);

  } else if (GenApi::intfIBoolean == node->GetPrincipalInterfaceType()) {
    Pylon::CBooleanParameter param(node);
    spec = g_param_spec_boolean(name.c_str(), display_name.c_str(),
                                desc.c_str(), param.GetValue(), flags);

  } else if (GenApi::intfIFloat == node->GetPrincipalInterfaceType()) {
    Pylon::CFloatParameter param(node);
    spec = g_param_spec_float(name.c_str(), display_name.c_str(), desc.c_str(),
                              param.GetMin(), param.GetMax(), param.GetValue(),
                              flags);

  } else if (GenApi::intfIString == node->GetPrincipalInterfaceType()) {
    Pylon::CStringParameter param(node);
    spec = g_param_spec_string(name.c_str(), display_name.c_str(), desc.c_str(),
                               param.GetValue(), flags);

  } else if (GenApi::intfIEnumeration == node->GetPrincipalInterfaceType()) {
    Pylon::CEnumParameter param(node);
    spec = g_param_spec_enum(name.c_str(), display_name.c_str(), desc.c_str(),
                             G_TYPE_INT, param.GetIntValue(), flags);

  } else {
    throw Pylon::GenericException("Cannot set param spec, invalid node",
                                  __FILE__, __LINE__);
  }

  return spec;
}
