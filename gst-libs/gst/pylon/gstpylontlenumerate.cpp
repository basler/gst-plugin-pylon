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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstpylondebug.h"
#include "gstpylontlenumerate.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>
#include <vector>

namespace {

std::string to_lower_ascii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

std::vector<std::string> parse_skip_tl_env(const char* raw_env) {
  std::vector<std::string> result;
  if (!raw_env || raw_env[0] == '\0') {
    return result;
  }

  std::stringstream ss(raw_env);
  std::string token;
  while (std::getline(ss, token, ',')) {
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      continue;
    }
    const auto last = token.find_last_not_of(" \t\r\n");
    std::string trimmed = to_lower_ascii(token.substr(first, last - first + 1));
    if (!trimmed.empty()) {
      result.push_back(std::move(trimmed));
    }
  }
  return result;
}

class ScopedTransportLayer {
 public:
  ScopedTransportLayer(Pylon::CTlFactory& factory, Pylon::ITransportLayer* tl)
      : factory_(factory), tl_(tl) {}
  ~ScopedTransportLayer() {
    if (tl_) {
      factory_.ReleaseTl(tl_);
    }
  }
  ScopedTransportLayer(const ScopedTransportLayer&) = delete;
  ScopedTransportLayer& operator=(const ScopedTransportLayer&) = delete;

  Pylon::ITransportLayer* get() const { return tl_; }

 private:
  Pylon::CTlFactory& factory_;
  Pylon::ITransportLayer* tl_;
};

}  // namespace

std::string gst_pylon_get_tl_friendly_name(const Pylon::String_t& tl_class) {
  const std::string class_str = tl_class.c_str();
  const std::string lower = to_lower_ascii(class_str);

  if (lower.find("camemu") != std::string::npos) {
    return "camemu";
  }
  if (lower.find("usb") != std::string::npos) {
    return "usb";
  }
  if (lower.find("gige") != std::string::npos) {
    return "gige";
  }
  if (lower.find("cxp") != std::string::npos) {
    return "cxp";
  }
  if (lower.find("cameralink") != std::string::npos) {
    return "cameralink";
  }

  std::string friendly = class_str;
  for (const char* token : {"Basler", "DeviceClass", "GenTl"}) {
    const auto pos = friendly.find(token);
    if (pos != std::string::npos) {
      friendly.erase(pos, std::strlen(token));
    }
  }
  friendly = to_lower_ascii(std::move(friendly));
  return friendly.empty() ? "unknown" : friendly;
}

void gst_pylon_enumerate_devices(Pylon::DeviceInfoList_t& device_list,
                                 const Pylon::DeviceInfoList_t* filter) {
  Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();

  const char* env_skip = std::getenv("PYLONSRC_SKIP_TRANSPORT_LAYERS");
  if (!env_skip || env_skip[0] == '\0') {
    env_skip = std::getenv("GST_PYLON_SKIP_TL");
  }

  const std::vector<std::string> skip_list = parse_skip_tl_env(env_skip);
  if (skip_list.empty()) {
    if (filter) {
      factory.EnumerateDevices(device_list, *filter);
    } else {
      factory.EnumerateDevices(device_list);
    }
    return;
  }

  device_list.clear();

  Pylon::TlInfoList_t tl_list;
  factory.EnumerateTls(tl_list);

  std::set<std::string> available_names;
  std::set<Pylon::String_t> processed_classes;

  for (const auto& tl_info : tl_list) {
    const Pylon::String_t& tl_class = tl_info.GetDeviceClass();
    if (!processed_classes.insert(tl_class).second) {
      continue;
    }

    const std::string friendly = gst_pylon_get_tl_friendly_name(tl_class);
    available_names.insert(friendly);

    if (std::find(skip_list.begin(), skip_list.end(), friendly) !=
        skip_list.end()) {
      GST_DEBUG("Skipping transport layer '%s' (%s)", friendly.c_str(),
                tl_class.c_str());
      continue;
    }

    try {
      ScopedTransportLayer tl_guard(factory, factory.CreateTl(tl_class));
      if (tl_guard.get()) {
        Pylon::DeviceInfoList_t tl_devices;
        if (filter) {
          tl_guard.get()->EnumerateDevices(tl_devices, *filter);
        } else {
          tl_guard.get()->EnumerateDevices(tl_devices);
        }
        GST_DEBUG("Enumerated %zu device(s) on transport layer '%s'",
                  tl_devices.size(), friendly.c_str());
        for (const auto& dev : tl_devices) {
          device_list.push_back(dev);
        }
      }
    } catch (const Pylon::GenericException& e) {
      GST_WARNING("Failed to enumerate transport layer '%s': %s",
                  friendly.c_str(), e.GetDescription());
    }
  }

  for (const auto& skip_name : skip_list) {
    if (available_names.find(skip_name) == available_names.end()) {
      GST_WARNING(
          "Unknown transport layer '%s' in PYLONSRC_SKIP_TRANSPORT_LAYERS",
          skip_name.c_str());
    }
  }
}
