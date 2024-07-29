#! /bin/env python3
import gi

gi.require_version("Gst", "1.0")
gi.require_version("GstVideo", "1.0")  # Import GstVideo
from gi.repository import Gst, GObject, GstVideo

import pygstpylon


# Initialize GStreamer
Gst.init(None)


# Define the probe function
def probe_callback(pad, info):
    # Get the buffer from the probe info
    buffer = info.get_buffer()

    if buffer:
        # Print buffer metadata
        print("Buffer Metadata:")

        # Get buffer duration
        duration = buffer.duration
        print(f"  Duration: {duration} nanoseconds")

        # Get buffer size
        size = buffer.get_size()
        print(f"  Size: {size} bytes")

        # Print any additional metadata (e.g., timestamps)
        pts = buffer.pts
        dts = buffer.dts
        print(f"  PTS: {pts} nanoseconds")
        print(f"  DTS: {dts} nanoseconds")

        # Access GstVideoMeta
        video_meta = GstVideo.buffer_get_video_meta(buffer)
        if video_meta:
            print("GstVideoMeta:")
            print(f"  Width: {video_meta.width}")
            print(f"  Height: {video_meta.height}")
            print(f"  Format: {video_meta.format}")
            print(f"  Number of Planes: {video_meta.n_planes}")
            print(f"  Stride: {video_meta.stride}")
            print(f"  Offsets: {video_meta.offset}")

        # Extract Pylon chunk data (if available)
        pylon_meta = pygstpylon.gst_buffer_get_pylon_meta(hash(buffer))
        if pylon_meta:
            print("Pylon Chunk Metadata:")
            print(f"   BlockID: {pylon_meta.block_id}")
            print(f"   Image Number: {pylon_meta.image_number}")
            print(f"   Offset X: {pylon_meta.offset_x}")
            print(f"   Offset Y: {pylon_meta.offset_y}")
            print(f"   Skipped Images: {pylon_meta.skipped_images}")
            print(f"   Stride: {pylon_meta.stride}")
            print(f"   Timestamp: {pylon_meta.timestamp}")
            print(f"   Chunks: {pylon_meta.chunks}")

    return Gst.PadProbeReturn.OK


# Create the GStreamer elements
pylon_source = Gst.ElementFactory.make("pylonsrc", "source")
fake_sink = Gst.ElementFactory.make("fakesink", "sink")

# Check if elements were created successfully
if not pylon_source or not fake_sink:
    print("Not all elements could be created.")
    exit(-1)

# Create an empty pipeline
pipeline = Gst.Pipeline.new("test-pipeline")

# Add elements to the pipeline
pipeline.add(pylon_source)
pipeline.add(fake_sink)

# Link the elements
pylon_source.link(fake_sink)

# Add a probe to the source pad of the pylonsrc element
src_pad = pylon_source.get_static_pad("src")
if src_pad:
    src_pad.add_probe(Gst.PadProbeType.BUFFER, probe_callback)


def stop_main_loop():
    print("Stopping the main loop after 10 seconds.")
    main_loop.quit()  # This will stop the main loop


# Start the pipeline
pipeline.set_state(Gst.State.PLAYING)

# Create the main loop
main_loop = GObject.MainLoop()

# Schedule the stop_main_loop function to be called after 10 seconds
GObject.timeout_add(1000, stop_main_loop)  # 1000 milliseconds = 1 seconds

print("Starting the main loop.")
main_loop.run()  # This will run until quit() is called

# Stop the pipeline
pipeline.set_state(Gst.State.NULL)
