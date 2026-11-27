/* Copyright (C) 2026 Basler AG
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstpylongendc.h"

#include <glib/gstdio.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#define CHECK(cond, msg)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
      return 1;                                                              \
    }                                                                        \
  } while (0)

namespace {

struct SyntheticGenDCGrabResult : public Pylon::GrabResult {
  void InitGenDC(const void* buf, size_t size, uint64_t block_id, uint64_t ts) {
    m_pBuffer = buf;
    m_BufferSize = size;
    m_PayloadSize = size;
    m_Status = Pylon::GrabStatus_Grabbed;
    m_PayloadType = Pylon::PayloadType_GenDC;
    m_PixelType = Pylon::PixelType_Undefined;
    m_BlockID = block_id;
    m_TimeStamp = ts;
  }
};

}  // namespace

int main(int argc, char** argv) {
  gst_init(&argc, &argv);
  Pylon::PylonAutoInitTerm auto_init;

  /* 1. Capture a real frame from CamEmu and serialize it as a valid GenDC 1.1
   *    container using Pylon's own CPylonDataContainer::Save(), then verify
   *    zero-copy resolution from a PayloadType_GenDC GrabResult. */
  Pylon::CDeviceInfo filter;
  filter.SetDeviceClass("BaslerCamEmu");
  Pylon::CBaslerUniversalInstantCamera cam(
      Pylon::CTlFactory::GetInstance().CreateFirstDevice(filter));
  cam.Open();
  cam.Width.SetValue(64);
  cam.Height.SetValue(48);
  cam.PixelFormat.SetValue("Mono8");

  cam.StartGrabbing(1);
  Pylon::CBaslerUniversalGrabResultPtr live_res;
  CHECK(cam.RetrieveResult(5000, live_res, Pylon::TimeoutHandling_Return),
        "RetrieveResult from CamEmu succeeded");
  CHECK(live_res && live_res->GrabSucceeded(), "CamEmu grab succeeded");

  /* Also verify regular PayloadType_Image resolution via
   * gst_pylon_resolve_grab_payload */
  {
    GstPylonResolvedPayload plain;
    GError* err = nullptr;
    CHECK(gst_pylon_resolve_grab_payload(live_res, false, plain, &err),
          "resolve regular PayloadType_Image succeeded");
    CHECK(err == nullptr, "no error on regular payload");
    CHECK(!plain.is_gendc, "regular payload is_gendc == false");
    CHECK(plain.data == live_res->GetBuffer(),
          "regular payload points directly to grab buffer");
    CHECK(plain.width == 64 && plain.height == 48,
          "regular payload geometry matches");
    CHECK(plain.size == 64 * 48, "regular payload size matches");
    CHECK(plain.stride == 64, "regular payload stride matches");
  }

  gchar* tmp_dir = g_dir_make_tmp("gstpylon-gendc-XXXXXX", nullptr);
  CHECK(tmp_dir != nullptr, "created temp directory");
  std::string gendc_path = std::string(tmp_dir) + "/frame.gendc";

  live_res->GetDataContainer().Save(gendc_path.c_str());
  const std::vector<uint8_t> expected_pixels(
      static_cast<const uint8_t*>(live_res->GetBuffer()),
      static_cast<const uint8_t*>(live_res->GetBuffer()) + 64 * 48);

  cam.StopGrabbing();
  cam.Close();

  std::ifstream ifs(gendc_path, std::ios::binary | std::ios::ate);
  CHECK(ifs.good(), "opened saved GenDC container");
  const size_t file_size = static_cast<size_t>(ifs.tellg());
  CHECK(file_size > 64 * 48,
        "GenDC container includes descriptor header + pixel payload");
  ifs.seekg(0);
  std::vector<uint8_t> gendc_bytes(file_size);
  ifs.read(reinterpret_cast<char*>(gendc_bytes.data()),
           static_cast<std::streamsize>(file_size));
  ifs.close();

  g_unlink(gendc_path.c_str());
  g_rmdir(tmp_dir);
  g_free(tmp_dir);

  /* 2. Construct a PayloadType_GenDC GrabResult whose top-level GetBuffer()
   *    points at the GenDC descriptor (NOT pixels) and whose top-level
   *    GetPixelType() is PixelType_Undefined, matching GeV 3.0 / USB3 GenDC. */
  SyntheticGenDCGrabResult gendc_grab;
  gendc_grab.InitGenDC(gendc_bytes.data(), gendc_bytes.size(), 42,
                       987654321ULL);

  Pylon::CPylonDataContainer container(gendc_grab);
  GstPylonResolvedPayload resolved;
  GError* err = nullptr;
  CHECK(gst_pylon_resolve_gendc_component(container, resolved, &err),
        "gst_pylon_resolve_gendc_component succeeded");
  CHECK(err == nullptr, "no error resolving GenDC container");
  CHECK(resolved.is_gendc, "resolved.is_gendc == true");
  CHECK(resolved.width == 64 && resolved.height == 48,
        "GenDC resolved width/height == 64x48");
  CHECK(resolved.size == 64 * 48, "GenDC resolved data size == 3072");
  CHECK(resolved.stride == 64, "GenDC resolved stride == 64");
  CHECK(resolved.pixel_type == Pylon::PixelType_Mono8,
        "GenDC resolved pixel_type == Mono8");
  CHECK(resolved.data != gendc_bytes.data(),
        "GenDC resolved data skips container descriptor header");
  CHECK(static_cast<const uint8_t*>(resolved.data) > gendc_bytes.data() &&
            static_cast<const uint8_t*>(resolved.data) + resolved.size <=
                gendc_bytes.data() + gendc_bytes.size(),
        "GenDC resolved data points zero-copy inside container buffer");
  CHECK(std::memcmp(resolved.data, expected_pixels.data(),
                    expected_pixels.size()) == 0,
        "GenDC resolved pixel payload matches original image byte-for-byte");

  /* 3. Verify empty/invalid container returns a clean GError instead of
   *    crashing or throwing an uncaught exception. */
  Pylon::CPylonDataContainer empty_container;
  GstPylonResolvedPayload bad_resolved;
  GError* bad_err = nullptr;
  CHECK(!gst_pylon_resolve_gendc_component(empty_container, bad_resolved,
                                           &bad_err),
        "empty GenDC container fails cleanly");
  CHECK(bad_err != nullptr, "empty GenDC container populates GError");
  g_error_free(bad_err);

  return 0;
}
