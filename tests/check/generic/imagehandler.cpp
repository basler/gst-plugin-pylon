/* Copyright (C) 2026 Basler AG
 *
 * Unit test for GstPylonImageHandler flush/disconnect/idle state handling.
 * Does not require a physical camera.
 */

#include "gstpylonimagehandler.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

#define CHECK(cond, msg)                       \
  do {                                         \
    if (!(cond)) {                             \
      std::fprintf(stderr, "FAIL: %s\n", msg); \
      failures++;                              \
    } else {                                   \
      std::printf("ok: %s\n", msg);            \
    }                                          \
  } while (0)

int main() {
  {
    GstPylonImageHandler handler;
    Pylon::CBaslerUniversalGrabResultPtr* grab = nullptr;
    handler.InterruptWaitForImage();
    auto result = handler.WaitForImage(&grab);
    CHECK(result == GstPylonImageHandlerResult::flushing,
          "InterruptWaitForImage wakes WaitForImage as flushing");
    CHECK(grab == nullptr, "flushing wait returns null grab pointer");
  }

  {
    GstPylonImageHandler handler;
    Pylon::CBaslerUniversalGrabResultPtr* grab = nullptr;
    handler.InterruptWaitForImage();
    handler.ClearInterrupt();
    handler.SignalDisconnect();
    auto result = handler.WaitForImage(&grab);
    CHECK(result == GstPylonImageHandlerResult::disconnected,
          "SignalDisconnect wakes WaitForImage as disconnected");
    CHECK(grab == nullptr, "disconnect wait returns null grab pointer");
  }

  {
    GstPylonImageHandler handler;
    Pylon::CBaslerUniversalGrabResultPtr* grab = nullptr;
    handler.InterruptWaitForImage();
    auto first = handler.WaitForImage(&grab);
    CHECK(first == GstPylonImageHandlerResult::flushing,
          "interrupt is sticky before ClearInterrupt");
    handler.ClearInterrupt();
    handler.SignalDisconnect();
    auto second = handler.WaitForImage(&grab);
    CHECK(second == GstPylonImageHandlerResult::disconnected,
          "ClearInterrupt allows a later disconnect wakeup");
  }

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All image handler tests passed\n");
  return EXIT_SUCCESS;
}
