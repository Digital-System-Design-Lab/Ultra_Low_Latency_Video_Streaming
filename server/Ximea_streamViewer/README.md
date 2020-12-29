
# discription:

This README is to connect ximea package api and gst-rtsp-server1.0 to make rtsp server using ximea camera.


# requirements:

1. Ximea API (for ximea camera)

API package to use ximea camera in Ubuntu environment should be installed.
If you install Ximea API package, you can use xiCamTool software, simple examples provided by ximea, 
and you can directly implement to use various options of ximea camera by using API.

1.1. Install by following the steps in "installation" section at the address below.Follow the steps for USB3 cameras, not PCIe Cameras.

	https://www.ximea.com/support/wiki/apis/XIMEA_Linux_Software_Package

1.2. Allows Plugdev group to access ximea camera.

1.2.1. If you hav plugdev when you run command below, go to step 1.2.3. If not, go to step 1.2.2.

1.2.2. 
	sudo gpasswd -a $USER plugdev
	sudo groupadd plugdev

1.2.3. Open "/etc/ld.so/conf.d/000XIMEA.conf" and Check it has following content. If not, add following content to the file using vi.

	/lib
	/usr/lib
	/usr/include

** If xiCamTool software does not work even after all the steps, enter the command below.

	sudo tee /sys/module/usbcore/parameters/usbfs_memory_mb >/dev/null <<<0

2. gtk , gstreamer1.0 , gstreamer-plugins-base1.0

	sudo apt-get install libgtk2.0-dev
	sudo apt-get install libgstreamer-plugins-base1.0-dev gstreamer1.0-x

3. gst-rtsp-server1.0

3.1. Download the ***same version*** as the current gstreamer version from the address below.

	https://launchpad.net/ubuntu/+source/gst-rtsp-server1.0

3.2. Follow README in gst-rtsp-server1.0 package.

3.3. Open terminal at "gst-rtsp-server-1.0/gst" and enter the command below. 

	sudo cp -r rtsp-server /usr/include/gstreamer-1.0/gst

3.4 
	sudo cp /home/nvidia/gstreamer/gst-rtsp-server-1.14.5/pkgconfig/gstreamer-rtsp-server.pc /usr/lib/aarch64-linux-gnu/pkgconfig/gstreamer-rtsp-server.pc
	sudo cp /home/nvidia/gstreamer/gst-rtsp-server-1.14.5/pkgconfig/gstreamer-rtsp-server-1.0.pc /usr/lib/aarch64-linux-gnu/pkgconfig/gstreamer-rtsp-server-1.0.pc


# setting guide:

1. Modify Makefile of StreamViewer example of ximea package as follows.

make file address : /ximea/package/examples/streamViewer/Makefile

before
```
ifeq($(GST10), 1)
	GSTMODULES=gstreamer-app-1.0 gstreamer-video-1.0
else
	GSTMODULES=gstreamer-app-0.10 gstreamer-interfaces-0.10
endif
```
after
```
ifeq($(GST10), 1)
	GSTMODULES=gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-rtsp-1.0 gstreamer-rtsp-server-1.0
else
	GSTMODULES=gstreamer-app-0.10 gstreamer-interfaces-0.10
endif
```
2. Open terminal at "/ximea/package/examples/streamViewer" and enter the command below. 

	sudo ln -s /usr/lib/aarch64-linux-gnu/libgstrtspserver-1.0.so.0.1405.0 /usr/lib/aarch64-linux-gnu/libgstrtspserver-1.0.so

3. Replace "/ximea/package/examples/streamViewer.c" with "streamViewer.c" in this repository.

4. make GST10=1

5. ./streamViewer

