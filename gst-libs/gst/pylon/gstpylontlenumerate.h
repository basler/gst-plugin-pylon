/* Copyright (C) 2026 Basler AG
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

#pragma once

#include <gst/pylon/gstpylon-prelude.h>
#include <gst/pylon/gstpylonincludes.h>

#include <string>

/* Convert a Pylon device class string (e.g. "BaslerUsb", "BaslerGigE") into a
 * normalized lowercase transport-layer token ("usb", "gige", "cxp", ...). */
EXT_PYLONSRC_API std::string gst_pylon_get_tl_friendly_name(
    const Pylon::String_t& tl_class);

/* Enumerate Pylon devices while honoring PYLONSRC_SKIP_TRANSPORT_LAYERS (or
 * GST_PYLON_SKIP_TL), e.g. "gige,cxp". Always releases created transport-layer
 * handles via CTlFactory::ReleaseTl(). */
EXT_PYLONSRC_API void gst_pylon_enumerate_devices(
    Pylon::DeviceInfoList_t& device_list,
    const Pylon::DeviceInfoList_t* filter = nullptr);
