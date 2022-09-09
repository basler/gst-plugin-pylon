import gi
import os
gi.require_version('Gst', '1.0')
from gi.repository import Gst

Gst.init()

pipe = Gst.parse_launch("pylonsrc name=src ! fakesink")

pylon = pipe.get_by_name("src")
cam = pylon.get_child_by_name("cam")

print("Modelname is: ", cam.get_property("DeviceModelName"))


def set_exposure(exp:float):
    cam.set_property("ExposureTime", exp)

def get_exposure() -> float:
    return cam.get_property("ExposureTime")


print(get_exposure())
set_exposure(get_exposure() + 100)
print(get_exposure())


