# GstPluginPylon

> A GStreamer plug-in for Basler cameras powered by Pylon SDK

Project under construction!

## Building the project

Make sure the dependencies are properly installed. In Debian-based
systems you can run the following commands:

```bash
# Meson build system.
# Remove older meson from APT and install newer PIP version
sudo apt remove meson
sudo -H python3 -m pip install meson

# GStreamer
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

Then proceed to configure the project. Check `meson_options.txt` for a
list of configuration options. On Debian-based systems, make sure you
configure the project as:

```bash
meson builddir --prefix /usr/
```

Build, test and install the project:

```bash
# Build
ninja -C builddir

# Test
ninja -C builddir test

# Install
sudo ninja -C builddir install
```

Finally, test for proper installation:

```bash
gst-inspect-1.0 pylonsrc
```

### Maintainer configuration

If you are a maintainer or plan to extend the plug-in, we recommend
the following configuration:

```
meson builddir --prefix /usr/ --werror --buildtype=debug -Dgobject-cast-checks=enabled -Dglib-asserts=enabled -Dglib-checks=enabled
```
