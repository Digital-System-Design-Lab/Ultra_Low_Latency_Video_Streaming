# Ultra_Low_Latency_Video_Streaming

## description
 It is a live video streaming system that divides an image into several horizontal slices and performs individual parallel streaming processing.

If the system is implemented with an nvidia jetson tx2 board equipped with multi-codec, parallel encoding and decoding can be used more efficiently.

### Folders that can be ignored in basic slice-based streaming system

```server/gstslicer(higherROIfps)```

```client/higherROIfps```

(These are for the optional system.)

## end-to-end latency
 
 With this system we measured the end-to-end latency of ***28.23ms***.
 
### Implementation equipment
-server : Nvidia Jetson TX2

-client : Nvidia Jetson TX2

-client monitor : BENQ 2411P (maximum 144hz)

-camera : Ximea MC031CG-SY-UB (using 100fps)

## Option

There is an optional slice-based ***higher Region-of-Interest fps*** video streaming system.

Two slices in the middle of the image divided into six horizontal slices are determined as Region-of-Interest. The fps of non-ROI slices is set to 1/4 of camera fps.

So you can benefit from network traffic.

If you want slice-based higher Region-of-Interest fps video streaming system, 
Set up using the following folders.

```server : use gstslicer(higherROIfps) instead of gstslicer```

```client : use higherROIfps instead of original```

