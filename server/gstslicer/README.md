# requirements:

	gstreamer-base-1.0
	gstreamer-1.0

sudo apt-get install libgstreamer-plugins-base1.0-dev

# build guide:

1. open terminal

2. gcc -fPIC $CPPFLAGS $(pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-controller-1.0 gstreamer-video-1.0) -c -o gstslicer.o gstslicer.c

3. gcc -shared -o libgstslicer.so gstslicer.o $(pkg-config --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-controller-1.0 gstreamer-video-1.0 )

4. sudo cp libgstslicer.so /usr/lib/aarch64-linux-gnu/gstreamer-1.0/libslicer.so
