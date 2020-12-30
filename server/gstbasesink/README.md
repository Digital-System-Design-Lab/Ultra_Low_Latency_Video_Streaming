### requirements:

1. You need to build and install gstreamer with source code at address below.

1.1. Build and install according to its README

1.2. If it still does not run after running "sudo make install", run following command to move .so file.
	sudo cp /usr/local/lib/libgstbase-1.0.so /usr/lib/aarch64-linux-gnu/libgstbase-1.0.so


### discription:

 This "GstBasesink" code has been optimized to reduce the waiting time that occurs before transmission to network.

 Replace "gstbasesink.c" file in the source code built above with the file from this repository and rebuild gstreamer.

### setting guide:

1. Replace gst-plugins-1.14.5/libs/gst/base/gstbasesink with "gstbasesink.c" in this repository.

2. Rebuild and reinstall gstreamer.

