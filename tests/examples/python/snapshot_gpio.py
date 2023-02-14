#! /usr/bin/python3

""" 
    Copyright (C) 2023 Basler AG

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
        1. Redistributions of source code must retain the above
            copyright notice, this list of conditions and the following
            disclaimer.
        2. Redistributions in binary form must reproduce the above
            copyright notice, this list of conditions and the following
            disclaimer in the documentation and/or other materials
            provided with the distribution.
        3. Neither the name of the copyright holder nor the names of
            its contributors may be used to endorse or promote products
            derived from this software without specific prior written
            permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
    OF THE POSSIBILITY OF SUCH DAMAGE. 
"""


import gi
import pygstpylon


gi.require_version("Gst", "1.0")
gi.require_version("GstBase", "1.0")
gi.require_version("GstVideo", "1.0")
gi.require_version("GObject", "2.0")
gi.require_version("GLib", "2.0")


def test_meta(value, userdata):
    print(value, userdata)


from gi.repository import Gst, GObject, GstBase, GstVideo, GLib

Gst.init(None)


class PylonGPIOSnapshot(GstBase.BaseTransform):
    __gstmetadata__ = (
        "PylonGPIOSnapshot",
        "Filter/Effect/Video",
        "filter that will pass a snapshot on edge of gpio ",
        "thies.moeller@baslerweb.com",
    )

    __gproperties__ = {
        "trigger-source": (
            int,
            "gpio line for trigger",
            "GPIO line on the camera to use for the trigger of snapshot",
            1,
            4,
            1,
            GObject.ParamFlags.READWRITE,
        ),
        "rising-edge": (
            bool,
            "use rising edge as trigger condition",
            "create snapshot on rising edge of input signal",
            True,
            GObject.ParamFlags.READWRITE,
        ),
    }

    __gsttemplates__ = (
        Gst.PadTemplate.new(
            "src", Gst.PadDirection.SRC, Gst.PadPresence.ALWAYS, Gst.Caps.new_any()
        ),
        Gst.PadTemplate.new(
            "sink", Gst.PadDirection.SINK, Gst.PadPresence.ALWAYS, Gst.Caps.new_any()
        ),
    )

    def __init__(self):
        super(PylonGPIOSnapshot, self).__init__()

        # Initialize properties before Base Class initialization
        self.trigger_source = 1
        self.rising_edge = True
        self.last_state = None

        self.set_passthrough(True)

    def do_get_property(self, prop):
        if prop.name == "trigger-source":
            return self.trigger_source
        elif prop.name == "rising_edge":
            return self.rising_edge
        else:
            raise AttributeError("unknown property %s" % prop.name)

    def do_set_property(self, prop, value):
        if prop.name == "trigger-source":
            self.trigger_source = value
        elif prop.name == "rising-edge":
            self.rising_edge = value
        else:
            raise AttributeError("unknown property %s" % prop.name)

    def do_transform_ip(self, buf):
        Gst.info("timestamp(buffer):%ld" % buf.pts)

        meta = pygstpylon.gst_buffer_get_pylon_meta(hash(buf))
        line_state = meta.chunks["ChunkLineStatusAll"]

        gpio_state = line_state & (1 << (self.trigger_source - 1))

        trigger = False

        if not self.last_state is None:
            if self.rising_edge:
                if self.last_state == False and gpio_state == True:
                    trigger = True
                else:
                    trigger = False
            else:
                if self.last_state == True and gpio_state == False:
                    trigger = True
                else:
                    trigger = False

        self.last_state = gpio_state
        if trigger:
            Gst.info(" snapshot")
            return Gst.FlowReturn.OK
        else:
            ts = buf.pts
            # check that the timestamp is valid
            if ts != -1:
                duration = buf.duration
                self.srcpad.push_event(Gst.Event.new_gap(ts, duration))
            return Gst.FlowReturn.CUSTOM_SUCCESS


GObject.type_register(PylonGPIOSnapshot)
__gstelementfactory__ = ("pylongpiosnapshot_py", Gst.Rank.NONE, PylonGPIOSnapshot)
