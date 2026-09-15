#!/usr/bin/env python3
"""pygstpylon unittest suite (API + camemu, including four parallel streams)."""

import unittest

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402

import pygstpylon  # noqa: E402

EMU_SERIALS = ("0815-0000", "0815-0001", "0815-0002", "0815-0003")
NUM_BUFFERS = 8
PULL_TIMEOUT_NS = 10 * Gst.SECOND


def pylon_meta(buffer):
    return pygstpylon.gst_buffer_get_pylon_meta(hash(buffer))


def make_pylonsrc_appsink_pipeline(serial, num_buffers):
    return Gst.parse_launch(
        "pylonsrc device-serial-number={serial} num-buffers={n} "
        "! video/x-raw,format=GRAY8,width=640,height=480 "
        "! appsink name=sink emit-signals=false sync=false".format(
            serial=serial, n=num_buffers
        )
    )


class TestPygstpylonApi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Gst.init(None)

    def test_version(self):
        self.assertTrue(pygstpylon.__version__)
        self.assertIsInstance(pygstpylon.__version__, str)

    def test_gst_pylon_meta_zeros(self):
        meta = pygstpylon.GstPylonMeta()
        self.assertEqual(meta.block_id, 0)
        self.assertEqual(meta.image_number, 0)
        self.assertEqual(meta.skipped_images, 0)
        self.assertEqual(meta.timestamp, 0)
        self.assertEqual(meta.stride, 0)
        self.assertEqual(meta.offset_x, 0)
        self.assertEqual(meta.offset_y, 0)
        self.assertEqual(meta.chunks, {})

    def test_raw_buffer_has_no_meta(self):
        buf = Gst.Buffer.new_allocate(None, 64, None)
        self.assertIsNone(pylon_meta(buf))

    def test_bad_hash_does_not_crash(self):
        self.assertIsNone(pygstpylon.gst_buffer_get_pylon_meta(0))
        with self.assertRaises(TypeError):
            pygstpylon.gst_buffer_get_pylon_meta("not-an-address")


class TestPygstpylonCamemu(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Gst.init(None)
        if Gst.ElementFactory.find("pylonsrc") is None:
            raise unittest.SkipTest("pylonsrc plugin not found")

    def test_single_camemu_meta(self):
        pipeline = make_pylonsrc_appsink_pipeline(EMU_SERIALS[0], NUM_BUFFERS)
        appsink = pipeline.get_by_name("sink")
        self.assertIsNotNone(appsink)

        pipeline.set_state(Gst.State.PLAYING)
        image_numbers = []
        timestamps = []
        held_meta = None
        try:
            for i in range(NUM_BUFFERS):
                sample = appsink.emit("try-pull-sample", PULL_TIMEOUT_NS)
                self.assertIsNotNone(
                    sample, "timed out after {} buffers".format(i)
                )
                buf = sample.get_buffer()
                meta = pylon_meta(buf)
                self.assertIsNotNone(meta, "missing GstPylonMeta")
                self.assertGreater(meta.stride, 0)
                self.assertIsInstance(meta.offset_x, int)
                self.assertIsInstance(meta.offset_y, int)
                self.assertIsInstance(meta.chunks, dict)
                image_numbers.append(meta.image_number)
                timestamps.append(meta.timestamp)
                if i == 0:
                    held_meta = meta
                    del sample
                    del buf
                    self.assertGreater(held_meta.stride, 0)
                    self.assertIsInstance(held_meta.chunks, dict)
        finally:
            pipeline.set_state(Gst.State.NULL)

        self.assertEqual(len(image_numbers), NUM_BUFFERS)
        self.assertEqual(image_numbers, sorted(image_numbers))
        self.assertEqual(len(set(image_numbers)), NUM_BUFFERS)
        self.assertEqual(timestamps, sorted(timestamps))

    def test_four_parallel_camemu(self):
        pipelines = []
        sinks = []
        try:
            for serial in EMU_SERIALS:
                pipeline = make_pylonsrc_appsink_pipeline(serial, NUM_BUFFERS)
                sink = pipeline.get_by_name("sink")
                self.assertIsNotNone(sink, "appsink missing for {}".format(serial))
                pipelines.append(pipeline)
                sinks.append(sink)

            for pipeline in pipelines:
                ret = pipeline.set_state(Gst.State.PLAYING)
                self.assertNotEqual(ret, Gst.StateChangeReturn.FAILURE)

            received = [0] * len(EMU_SERIALS)
            while min(received) < NUM_BUFFERS:
                progressed = False
                for i, sink in enumerate(sinks):
                    if received[i] >= NUM_BUFFERS:
                        continue
                    sample = sink.emit("try-pull-sample", PULL_TIMEOUT_NS)
                    self.assertIsNotNone(
                        sample,
                        "timed out on {} after {} buffers".format(
                            EMU_SERIALS[i], received[i]
                        ),
                    )
                    meta = pylon_meta(sample.get_buffer())
                    self.assertIsNotNone(
                        meta, "missing GstPylonMeta on {}".format(EMU_SERIALS[i])
                    )
                    self.assertGreater(meta.stride, 0)
                    self.assertIsInstance(meta.chunks, dict)
                    received[i] += 1
                    progressed = True
                self.assertTrue(progressed)
        finally:
            for pipeline in pipelines:
                pipeline.set_state(Gst.State.NULL)

        self.assertEqual(received, [NUM_BUFFERS] * len(EMU_SERIALS))


if __name__ == "__main__":
    unittest.main()
