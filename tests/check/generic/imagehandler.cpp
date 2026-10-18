/* Copyright (C) 2026 Basler AG
 *
 * Unit test for GstPylonImageHandler flush/disconnect/idle state machine
 * and its pending frame queue. Does not require a physical camera. The
 * frame order checks take real grab results from a pylon camera emulator
 * (PYLON_CAMEMU) and are skipped if no emulator is found.
 */

#include "../../ext/pylon/gstpylonimagehandler.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <vector>

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

using GrabResult = Pylon::CBaslerUniversalGrabResultPtr;

/* Takes the next pending frame. Returns false, with a null grab pointer, if
 * no frame is pending: the blocked wait is then released by an interrupt
 * that is cleared again. */
static bool TakeFrame(GstPylonImageHandler& handler, GrabResult** grab) {
  auto wait = std::async(std::launch::async, [&handler, grab] {
    return handler.WaitForImage(grab);
  });
  if (wait.wait_for(std::chrono::milliseconds(500)) ==
      std::future_status::ready) {
    return wait.get() == GstPylonImageHandlerResult::ok;
  }
  handler.InterruptWaitForImage();
  wait.get();
  handler.ClearInterrupt();
  *grab = nullptr;
  return false;
}

/* Number of frames that can be taken before the handler runs empty */
static int CountPendingFrames(GstPylonImageHandler& handler) {
  int count = 0;
  GrabResult* grab = nullptr;
  while (TakeFrame(handler, &grab)) {
    delete grab;
    count++;
  }
  return count;
}

/* Takes the next frame and checks it is the expected grab result */
static bool TakeFrameIs(GstPylonImageHandler& handler,
                        const GrabResult& expected) {
  GrabResult* grab = nullptr;
  bool same = TakeFrame(handler, &grab) && grab && grab->IsValid() &&
              (*grab)->GetID() == expected->GetID();
  delete grab;
  return same;
}

static void TestQueueWithoutCamera() {
  Pylon::CBaslerUniversalInstantCamera camera;
  GrabResult empty;

  {
    GstPylonImageHandler handler;
    CHECK(handler.GetMaxPendingFrames() == 1,
          "max pending frames defaults to 1");
    handler.SetMaxPendingFrames(0);
    CHECK(handler.GetMaxPendingFrames() == 1,
          "max pending frames is at least 1");
  }

  {
    GstPylonImageHandler handler;
    handler.OnImageGrabbed(camera, empty);
    handler.OnImageGrabbed(camera, empty);
    CHECK(CountPendingFrames(handler) == 1,
          "default keeps a single pending frame");
  }

  {
    GstPylonImageHandler handler;
    handler.SetMaxPendingFrames(3);
    for (int i = 0; i < 5; i++) {
      handler.OnImageGrabbed(camera, empty);
    }
    CHECK(CountPendingFrames(handler) == 3, "queue is bounded");
  }

  {
    GstPylonImageHandler handler;
    GrabResult* grab = nullptr;
    handler.SetMaxPendingFrames(3);
    handler.OnImageGrabbed(camera, empty);
    handler.OnImageGrabbed(camera, empty);
    handler.InterruptWaitForImage();
    CHECK(handler.WaitForImage(&grab) == GstPylonImageHandlerResult::flushing,
          "interrupt wins over queued frames");
    handler.ClearInterrupt();
    CHECK(CountPendingFrames(handler) == 0, "interrupt clears the queue");
  }

  {
    GstPylonImageHandler handler;
    handler.SetMaxPendingFrames(3);
    handler.InterruptWaitForImage();
    handler.OnImageGrabbed(camera, empty);
    handler.ClearInterrupt();
    CHECK(CountPendingFrames(handler) == 0,
          "frames are dropped while interrupted");
  }

  {
    GstPylonImageHandler handler;
    GrabResult* grab = nullptr;
    handler.SetMaxPendingFrames(3);
    handler.OnImageGrabbed(camera, empty);
    handler.OnImageGrabbed(camera, empty);
    handler.SignalDisconnect();
    CHECK(
        handler.WaitForImage(&grab) == GstPylonImageHandlerResult::disconnected,
        "disconnect wins over queued frames");
  }
}

static void TestQueueOrderWithEmulator() {
  Pylon::CBaslerUniversalInstantCamera camera;
  std::vector<GrabResult> frames(5);

  try {
    Pylon::CDeviceInfo info;
    info.SetDeviceClass(Pylon::BaslerCamEmuDeviceClass);
    camera.Attach(Pylon::CTlFactory::GetInstance().CreateFirstDevice(info));
    camera.Open();
    camera.StartGrabbing(Pylon::GrabStrategy_OneByOne);
    for (auto& frame : frames) {
      camera.RetrieveResult(5000, frame, Pylon::TimeoutHandling_ThrowException);
    }
  } catch (const Pylon::GenericException& e) {
    std::printf("skip: frame order checks, no camera emulator (%s)\n",
                e.GetDescription());
    return;
  }

  {
    GstPylonImageHandler handler;
    handler.OnImageGrabbed(camera, frames[0]);
    handler.OnImageGrabbed(camera, frames[1]);
    CHECK(TakeFrameIs(handler, frames[1]), "default keeps the newest frame");
  }

  {
    GstPylonImageHandler handler;
    handler.SetMaxPendingFrames(3);
    for (int i = 0; i < 3; i++) {
      handler.OnImageGrabbed(camera, frames[i]);
    }
    bool in_order = true;
    for (int i = 0; i < 3; i++) {
      in_order = TakeFrameIs(handler, frames[i]) && in_order;
    }
    CHECK(in_order, "queue returns frames in grab order");
  }

  {
    GstPylonImageHandler handler;
    handler.SetMaxPendingFrames(3);
    for (auto& frame : frames) {
      handler.OnImageGrabbed(camera, frame);
    }
    bool newest = true;
    for (int i = 2; i < 5; i++) {
      newest = TakeFrameIs(handler, frames[i]) && newest;
    }
    CHECK(newest, "full queue drops the oldest frames");
  }

  {
    GstPylonImageHandler handler;
    handler.SetMaxPendingFrames(3);
    handler.OnImageGrabbed(camera, frames[0]);
    handler.OnImageGrabbed(camera, frames[1]);
    handler.InterruptWaitForImage();
    handler.ClearInterrupt();
    handler.OnImageGrabbed(camera, frames[2]);
    CHECK(TakeFrameIs(handler, frames[2]) && CountPendingFrames(handler) == 0,
          "frames queued before an interrupt are not delivered");
  }

  frames.clear();
  camera.StopGrabbing();
  camera.Close();
}

int main() {
  Pylon::PylonAutoInitTerm auto_init_term;

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
    /* Sticky interrupt: wait returns flushing again until ClearInterrupt */
    auto first = handler.WaitForImage(&grab);
    CHECK(first == GstPylonImageHandlerResult::flushing,
          "interrupt is sticky before ClearInterrupt");
    handler.ClearInterrupt();
    handler.SignalDisconnect();
    auto second = handler.WaitForImage(&grab);
    CHECK(second == GstPylonImageHandlerResult::disconnected,
          "ClearInterrupt allows a later disconnect wakeup");
  }

  TestQueueWithoutCamera();
  TestQueueOrderWithEmulator();

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("All image handler tests passed\n");
  return EXIT_SUCCESS;
}
