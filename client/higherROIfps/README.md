
### discription:

 This is to install and optimize gstreamer "compositor" plugin which combines divided image using "slicer" plugin on the server into one image before displaying it on the monitor.

### requirements:

1. gstreamer1.0 , gstreamer-plugins-base1.0

	sudo apt-get install libgstreamer-plugins-base1.0-dev gstreamer1.0-x
  
1. You need to build and install gstreamer1.0 with source code at address below.

1.1. Build and install according to its README

1.2. If it still does not run after running "sudo make install", run following command to move .so file.
	sudo cp /usr/local/lib/libgstbase-1.0.so /usr/lib/aarch64-linux-gnu/libgstbase-1.0.so

2. You need to build and install gst-plugins-bad-1.14.5 with source code at address below.

https://launchpad.net/ubuntu/+source/gst-plugins-bad1.0/1.14.5-0ubuntu1~18.04.1 

2.1. Build and install according to its README

2.2. If it still does not run after running "sudo make install", run following command to move .so file.

  sudo cp /usr/local/lib/libgstbadvideo-1.0.so /usr/lib/aarch64-linux-gnu/libgstbadvideo-1.0.so 

### setting guide:

1. Replace "gstreamer-1.14.5/libs/gst/base/gstaggregator.c" with "gstaggregator.c" in this repository.

2. Rebuild and reinstall gstreamer1.0.

3. Replace "gst-plugins-bad-1.14.5/gst-libs/gst/video/gstvideoaggregator.c" with "gstvideoaggregator.c" in this repository.

4. Rebuild and reinstall gst-plugins-bad-1.14.5.

5. Open terminal

6. After running streamViewer on the server system, you can run "slice-based streaming system" by using the following command on terminal of the client system.

gst-launch-1.0 nvcompositor name=comp1 sink_0::alpha=1 sink_0::xpos=0 sink_0::ypos=0 sink_0::width=1024 sink_0::height=128 sink_1::alpha=1 sink_1::xpos=0 sink_1::ypos=128 sink_1::width=1024 sink_1::height=128 ! nvvidconv ! "video/x-raw(memory:NVMM)" ! nvoverlaysink name=sink0 overlay-w=1024 overlay-h=256 overlay-x=0 overlay-y=256 sync=false async=false nvcompositor name=comp2 sink_0::alpha=1 sink_0::xpos=0 sink_0::ypos=0 sink_0::width=1024 sink_0::height=128 sink_1::alpha=1 sink_1::xpos=0 sink_1::ypos=128 sink_1::width=1024 sink_1::height=128 sink_2::alpha=1 sink_2::xpos=0 sink_2::ypos=512 sink_2::width=1024 sink_2::height=128 sink_3::alpha=1 sink_3::xpos=0 sink_3::ypos=640 sink_3::width=1024 sink_3::height=128 ! nvvidconv ! "video/x-raw(memory:NVMM)" ! nvoverlaysink name=sink1 overlay-w=1024 overlay-h=768 overlay-x=0 overlay-y=0 sync=false async=false rtspsrc location=rtsp://000.000.000.000:0000/test name=rtsp rtsp. ! 'application/x-rtp, payload=(int)96' ! rtpvp8depay ! nvv4l2decoder name=dec0 enable-max-performance=true ! comp2. rtsp. ! 'application/x-rtp, payload=(int)97' ! rtpvp8depay ! nvv4l2decoder name=dec1 enable-max-performance=true ! comp2. rtsp. ! 'application/x-rtp, payload=(int)98' ! rtpvp8depay ! nvv4l2decoder name=dec2 enable-max-performance=true ! comp1. rtsp. ! 'application/x-rtp, payload=(int)99' ! rtpvp8depay ! nvv4l2decoder name=dec3 enable-max-performance=true ! comp1. rtsp. ! 'application/x-rtp, payload=(int)100' ! rtpvp8depay ! nvv4l2decoder name=dec4 enable-max-performance=true ! comp2. rtsp. ! 'application/x-rtp, payload=(int)101' ! rtpvp8depay ! nvv4l2decoder name=dec5 enable-max-performance=true ! comp2.


