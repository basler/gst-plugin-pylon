#
# Regular cron jobs for the gst-plugin-pylon package
#
0 4	* * *	root	[ -x /usr/bin/gst-plugin-pylon_maintenance ] && /usr/bin/gst-plugin-pylon_maintenance
