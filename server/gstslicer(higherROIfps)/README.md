### requirements:

	gstreamer-base-1.0
	gstreamer-1.0

sudo apt-get install libgstreamer-plugins-base1.0-dev gstreamer1.0-x 


### discription:

 Two slices in the middle of the image divided into six horizontal slices are determined as Region-of-Interest. The fps of non-ROI slices is set to 1/4 of camera fps.
 That is, Only one of four consecutive frames is all delivered to next plugin. For the remaining 3 frames, only ROI slices are delivered to next plugin and nonROI slices are not delivered to next plugin and are deleted.

### build guide:

1. open terminal

2. gcc -fPIC $CPPFLAGS $(pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-controller-1.0 gstreamer-video-1.0) -c -o gstslicer.o gstslicer.c

3. gcc -shared -o libgstslicer.so gstslicer.o $(pkg-config --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-controller-1.0 gstreamer-video-1.0 )

4. sudo cp libgstslicer.so /usr/lib/aarch64-linux-gnu/gstreamer-1.0/libslicer.so
