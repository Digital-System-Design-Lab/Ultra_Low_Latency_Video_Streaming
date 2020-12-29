#include <sys/time.h>
#include <string.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <pthread.h>
#include <string.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif
#include <gst/app/gstappsrc.h>
#if GST_CHECK_VERSION(1,0,0)
#include <gst/video/videooverlay.h>
#define VIDEOCONVERT "videoconvert"
#else
#include <gst/interfaces/xoverlay.h>
#define VIDEOCONVERT "ffmpegcolorspace"
#endif
#include <m3api/xiApi.h>
#include <chrono>
#define DEFAULT_RTSP_PORT "8554"

#define WIDTH 1024
#define HEIGHT 768
#define FPS 100

#define EXPOSURE 0//(to set default:0) [microsec]

//ximea queue size
int queuesize=1;

int i = 0;
class StopWatch{
public:
void Start(){
	t0 =  std::chrono::high_resolution_clock::now();
	}
private:
std::chrono::high_resolution_clock::time_point t0;
};
static char *port = (char *) DEFAULT_RTSP_PORT;

BOOLEAN acquire, quitting, render = TRUE;
int maxcx, maxcy, roix0, roiy0, roicx, roicy;
pthread_t videoThread;
HANDLE handle = INVALID_HANDLE_VALUE;
guintptr window_handle;

GstBuffer *buffer;
XI_IMG image;

GstBusSyncReply bus_sync_handler(GstBus* bus, GstMessage* message, gpointer) {
#if GST_CHECK_VERSION(1,0,0)
	if(!gst_is_video_overlay_prepare_window_handle_message(message))
#else
	if(GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT || !gst_structure_has_name(message->structure, "prepare-xwindow-id"))
#endif
		return GST_BUS_PASS;
#if GST_CHECK_VERSION(1,0,0)
	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message)), window_handle);
#elif GST_CHECK_VERSION(0,10,31)
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(GST_MESSAGE_SRC(message)), window_handle);
#else
	gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(message)), window_handle);
#endif
	gst_message_unref(message);
	return GST_BUS_DROP;
}

#if defined(GDK_WINDOWING_X11)
#define VIDEOSINK "xvimagesink"
void video_widget_realize_cb(GtkWidget* widget, gpointer) {
	GdkWindow *window = gtk_widget_get_window(widget);
	if (!gdk_window_ensure_native(window))
		g_error ("Couldn't create native window needed for GstXOverlay!");
	window_handle = GDK_WINDOW_XID(window);
}
#elif defined(GDK_WINDOWING_QUARTZ)
#define VIDEOSINK "osxvideosink"
void video_widget_realize_cb(GtkWidget*, gpointer);
#else
#error Unsupported GDK backend
#endif

inline unsigned long getcurus() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec * 1000000 + now.tv_usec;
}

gboolean close_cb(GtkWidget*, GdkEvent*, gpointer quit) {
	quitting = quit ? TRUE : FALSE;
	if(videoThread)
		acquire = FALSE;
	else
		gtk_main_quit();
	return TRUE;
}

void* videoDisplay(void*) {
	GtkWidget *videoWindow;
	GdkScreen *screen;
	GstElement *pipeline, *appsrc, *scale;
	GstFlowReturn ret;
	GstBus *bus;
	GstCaps *caps = 0;
	GstCaps *size_caps = 0;
	int max_width, max_height;
	int prev_width = -1;
	int prev_height = -1;
	unsigned long frames = 0;
	unsigned long prevframes = 0;
	unsigned long lostframes = 0;
	unsigned long curtime, prevtime;
	long lastframe = -1;
	gchar title[256];


	/* ximea camera software setting */

	//using black and white image ( with minimal image processing time in cpu )
	//use "XI_RGB32" to use RGB image 
	XI_IMG_FORMAT prev_format = XI_RAW8;
        xiSetParamInt(handle, XI_PRM_IMAGE_DATA_FORMAT,XI_RAW8);
	//setting resolution
	xiSetParamInt(handle, XI_PRM_HEIGHT,HEIGHT);
	xiSetParamInt(handle, XI_PRM_WIDTH,WIDTH);

	int payload=0;
	xiGetParamInt(handle, XI_PRM_IMAGE_PAYLOAD_SIZE, &payload);
	printf("\n\n\nxiGetParam (payload) %d",payload);

	// ---------------------------------------------------
	// select transport buffer size depending on payload

	int transport_buffer_size_default = 0;
	int transport_buffer_size_increment = 0;
	int transport_buffer_size_minimum = 0;
	// get default transport buffer size - that should be OK on all controllers
	xiGetParamInt(handle, XI_PRM_ACQ_TRANSPORT_BUFFER_SIZE, &transport_buffer_size_default);
	printf("\nxiGetParamInt (transport buffer size default) %d",transport_buffer_size_default);
	xiGetParamInt(handle, XI_PRM_ACQ_TRANSPORT_BUFFER_SIZE XI_PRM_INFO_INCREMENT, 	&transport_buffer_size_increment);
	printf("\nxiGetParamInt (transport buffer size increment) %d",transport_buffer_size_increment);
	xiGetParamInt(handle, XI_PRM_ACQ_TRANSPORT_BUFFER_SIZE XI_PRM_INFO_MIN, &transport_buffer_size_minimum);
    	xiSetParamInt(handle, XI_PRM_ACQ_TRANSPORT_BUFFER_SIZE, transport_buffer_size_minimum);

// check if payload size is less than default transport buffer size
if(payload < transport_buffer_size_default + transport_buffer_size_increment)
{
    // use optimized transport buffer size, as nearest increment to payload
    int transport_buffer_size = payload;
    if (transport_buffer_size_increment)
    {
        // round up to nearest increment
        int remainder = transport_buffer_size % transport_buffer_size_increment;
        if (remainder)
            transport_buffer_size += transport_buffer_size_increment - remainder;
    }
    // check the minimum
    if (transport_buffer_size < transport_buffer_size_minimum)
        transport_buffer_size = transport_buffer_size_minimum;
    xiSetParamInt(handle, XI_PRM_ACQ_TRANSPORT_BUFFER_SIZE, transport_buffer_size);
    printf("\nxiSetParam (transport buffer size) %d",transport_buffer_size);
}

//use the most recent frame in ximea camera queue
xiSetParamInt(handle,XI_PRM_RECENT_FRAME,XI_ON);

//setting camera exposure time (default 10000 microsec)
if(EXPOSURE > 0){
xiSetParamInt(handle,XI_PRM_EXPOSURE,EXPOSURE);}

	image.size = sizeof(XI_IMG);
	image.bp = NULL;
	image.bp_size = 0;
	
	if(xiStartAcquisition(handle) != XI_OK)
		goto exit;

	while(acquire) {
		if(xiGetImage(handle, 30, &image) == XI_OK)
			break;
	}
exit:
	acquire = FALSE;

	return 0;
}

struct ctrl_window {
    GtkWidget *window, *boxmain, *boxgpi, *boxx, *boxy, *gpi1, *gpi2, *gpi3, *gpi4, *labelexp, *exp, *labelgain, *gain, *labelx0, *x0, *labely0, *y0, *labelcx, *cx, *labelcy, *cy, *raw, *show, *run;
};

//unused
gboolean time_handler(ctrl_window *ctrl) {
	int level = 0;
	if(acquire && handle != INVALID_HANDLE_VALUE) {
		xiSetParamInt(handle, XI_PRM_GPI_SELECTOR, 1);
		xiGetParamInt(handle, XI_PRM_GPI_LEVEL, &level);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl->gpi1), level);
		xiSetParamInt(handle, XI_PRM_GPI_SELECTOR, 2);
		xiGetParamInt(handle, XI_PRM_GPI_LEVEL, &level);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl->gpi2), level);
		xiSetParamInt(handle, XI_PRM_GPI_SELECTOR, 3);
		xiGetParamInt(handle, XI_PRM_GPI_LEVEL, &level);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl->gpi3), level);
	}
	gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(ctrl->run), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctrl->run)) != acquire);
	gtk_widget_set_sensitive(ctrl->boxx, acquire);
	gtk_widget_set_sensitive(ctrl->boxy, acquire);
	gtk_widget_set_sensitive(ctrl->exp, acquire);
	gtk_widget_set_sensitive(ctrl->gain, acquire);
	return TRUE;
}
//unused
gboolean update_x0(GtkAdjustment *adj, gpointer) {
	roix0 = gtk_adjustment_get_value(adj);
	if(roicx + roix0 > maxcx){
		roix0 = maxcx - roicx;
		gtk_adjustment_set_value(adj, roix0);
	}
	xiSetParamInt(handle, XI_PRM_OFFSET_X, roix0);
	return TRUE;
}
//unused
gboolean update_y0(GtkAdjustment *adj, gpointer) {
	roiy0 = gtk_adjustment_get_value(adj);
	if(roicy + roiy0 > maxcy){
		roiy0 = maxcy - roicy;
		gtk_adjustment_set_value(adj, roiy0);
	}
	xiSetParamInt(handle, XI_PRM_OFFSET_Y, roiy0);
	return TRUE;
}
//unused
gboolean update_cx(GtkAdjustment *adj, gpointer) {
	roicx = gtk_adjustment_get_value(adj);
	if(roix0 + roicx > maxcx){
		roicx = maxcx - roix0;
		gtk_adjustment_set_value(adj, roicx);
	}
	xiSetParamInt(handle, XI_PRM_WIDTH, roicx);
	return TRUE;
}
//unused
gboolean update_cy(GtkAdjustment *adj, gpointer) {
	roicy = gtk_adjustment_get_value(adj);
	if(roiy0 + roicy > maxcy) {
		roicy = maxcy - roiy0;
		gtk_adjustment_set_value(adj, roicy);
	}
	xiSetParamInt(handle, XI_PRM_HEIGHT, roicy);
	return TRUE;
}
//unused
gboolean update_exposure(GtkAdjustment *adj, gpointer) {
	xiSetParamInt(handle, XI_PRM_EXPOSURE, 1000*gtk_adjustment_get_value(adj));		
	return TRUE;
}
//unused
gboolean update_gain(GtkAdjustment *adj, gpointer) {
	xiSetParamFloat(handle, XI_PRM_GAIN, gtk_adjustment_get_value(adj));
	return TRUE;
}
//unused
gboolean update_raw(GtkToggleButton *raw, ctrl_window *ctrl) {
	if(handle != INVALID_HANDLE_VALUE) {
		float mingain, maxgain;
		xiSetParamInt(handle, XI_PRM_IMAGE_DATA_FORMAT, gtk_toggle_button_get_active(raw) ? XI_RAW8 : XI_RGB32);
		xiGetParamFloat(handle, XI_PRM_GAIN XI_PRM_INFO_MIN, &mingain);
		xiGetParamFloat(handle, XI_PRM_GAIN XI_PRM_INFO_MAX, &maxgain);
		xiGetParamInt(handle, XI_PRM_WIDTH XI_PRM_INFO_MAX, &maxcx);
		xiGetParamInt(handle, XI_PRM_HEIGHT XI_PRM_INFO_MAX, &maxcy);
		roicx = maxcx;
		roicy = maxcy;
		roix0 = 0;
		roiy0 = 0;
		gtk_adjustment_configure(gtk_range_get_adjustment(GTK_RANGE(ctrl->gain)), mingain, mingain, maxgain, 0.1, 1, 0);
		gtk_adjustment_configure(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl->x0)), roix0, 0, maxcx-4, 2, 20, 0);
		gtk_adjustment_configure(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl->y0)), roiy0, 0, maxcy-2, 2, 20, 0);
		gtk_adjustment_configure(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl->cx)), roicx, 4, maxcx, 4, 20, 0);
		gtk_adjustment_configure(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl->cy)), roicy, 2, maxcy, 2, 20, 0);
		xiSetParamFloat(handle, XI_PRM_GAIN, mingain);
		xiSetParamInt(handle, XI_PRM_OFFSET_X, roix0);
		xiSetParamInt(handle, XI_PRM_OFFSET_Y, roiy0);
		xiSetParamInt(handle, XI_PRM_WIDTH, roicx);
		xiSetParamInt(handle, XI_PRM_HEIGHT, roicy);
		//exposure doesn't seem to be affected by format change
	}
	return TRUE;
}
//unused
gboolean update_show(GtkToggleButton *show, gpointer) {
	render = gtk_toggle_button_get_active(show);
	return TRUE;
}

//unused
gboolean update_run(GtkToggleButton *run, ctrl_window *ctrl) {
	gtk_toggle_button_set_inconsistent(run, false);
	acquire = gtk_toggle_button_get_active(run);
	if(acquire && handle == INVALID_HANDLE_VALUE) {
		DWORD nIndex = 0;
		char* env = getenv("CAM_INDEX");
		if(env) {
			nIndex = atoi(env);
		}
		DWORD tmp;
		xiGetNumberDevices(&tmp); //rescan available devices
		if(xiOpenDevice(nIndex, &handle) != XI_OK) {
			printf("Couldn't setup camera!\n");
			acquire = FALSE;
			return TRUE;
		}
		update_raw(GTK_TOGGLE_BUTTON(ctrl->raw), ctrl);
		int isColor = 0;
		xiGetParamInt(handle, XI_PRM_IMAGE_IS_COLOR, &isColor);
		if(isColor)
			xiSetParamInt(handle, XI_PRM_AUTO_WB, 1);
		xiSetParamInt(handle, XI_PRM_EXPOSURE, 10000);
		gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(ctrl->exp)), 10);
		if(pthread_create(&videoThread, NULL, videoDisplay, NULL))
			exit(1);
	}
	gtk_widget_set_sensitive(ctrl->boxx, acquire);
	gtk_widget_set_sensitive(ctrl->boxy, acquire);
	gtk_widget_set_sensitive(ctrl->exp, acquire);
	gtk_widget_set_sensitive(ctrl->gain, acquire);
	return TRUE;
}


//unused
gboolean start_cb(ctrl_window* ctrl) {
	//start acquisition
	update_run(GTK_TOGGLE_BUTTON(ctrl->run), ctrl);
	return FALSE;
}


typedef struct
{
  gboolean white;
  GstClockTime timestamp;
} MyContext;


  struct timeval start_point,end_point;

  double operating;


  struct timeval fpsstart,fpsend;

  double fpstime;
double realfps=0.0;
 
int framenumber=0;
double diffsum=0;

/* called when we need to give data to appsrc */
static void
need_data (GstElement * appsrc,  guint unused, MyContext * ctx)
{
std::chrono::time_point<std::chrono::system_clock> start, end;
start = std::chrono::system_clock::now();
  	GstBuffer *buffer;
	  
 	 guint size;
 	 GstFlowReturn ret;


	XI_IMG image;

XI_RETURN stat =XI_OK;
	image.size = sizeof(XI_IMG);
	image.bp = NULL;
	image.bp_size;

	//get image from ximea camera
	xiGetImage(handle, 30, &image);
  
uint64_t value=0;
DWORD tsize=sizeof(value);
XI_PRM_TYPE type=xiTypeInteger64;
xiGetParam(handle,XI_PRM_TIMESTAMP,&value,&tsize,&type);

	//point--------------------------------------------------------------------------
	unsigned long buffer_size = WIDTH*HEIGHT*(image.frm == XI_RAW8 ? 1 : 4);

	//copy ximea image buffer (XI_IMG type) to gstreamer image buffer (GstBuffer type)
#if GST_CHECK_VERSION(1,0,0)
	buffer = gst_buffer_new_allocate(0, buffer_size, 0);
#else
                                buffer = gst_buffer_new_and_alloc(buffer_size);
#endif
                                for(int i = 0; i < image.height; i++)
#if GST_CHECK_VERSION(1,0,0)
                                        gst_buffer_fill(buffer,
#else
                                        memcpy(GST_BUFFER_DATA(buffer)+
#endif
                                                        i*image.width*(image.frm == XI_RAW8 ? 1 : 4),
                                                        (guint8*)image.bp+i*(image.width*(image.frm == XI_RAW8 ? 1 : 4)+image.padding_x),
                                                        image.width*(image.frm == XI_RAW8 ? 1 : 4));


GST_BUFFER_TIMESTAMP(buffer) = GST_CLOCK_TIME_NONE;


         ctx->white = !ctx->white;

	 /* increment the timestamp every 1/60 second */
 	 GST_BUFFER_PTS (buffer) = ctx->timestamp;

	//set camera fps by adjusting GST_BUFFER_DURATION parameter of gstreamer buffer
 	 GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, FPS);
	 ctx->timestamp += GST_BUFFER_DURATION (buffer);

	//push image buffer to next plugin
       	ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

}

/* called when a new media pipeline is constructed. We can query the
 * pipeline and configure our appsrc */
static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
    gpointer user_data)
{
  GstElement *element, *appsrc;
  MyContext *ctx;
	GstCaps *caps = 0;
	GstCaps *size_caps = 0;


  /* get the element used for providing the streams of the media */
  element = gst_rtsp_media_get_element (media);

  /* get our appsrc, we named it 'mysrc' with the name property */
  appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (element), "mysrc");

  /* this instructs appsrc that we will be dealing with timed buffer */
  gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
/* configure the caps of the video */
  g_object_set (G_OBJECT (appsrc), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "GRAY8",
          "width", G_TYPE_INT, WIDTH,
          "height", G_TYPE_INT, HEIGHT,
	"bpp", G_TYPE_INT,8,
	"depth", G_TYPE_INT, 8,
          "framerate", GST_TYPE_FRACTION, 0, 1, NULL), NULL);
  //gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
  ctx = g_new0 (MyContext, 1);
  ctx->white = FALSE;
  ctx->timestamp = 0;
  /* make sure ther datais freed when the media is gone */
  g_object_set_data_full (G_OBJECT (media), "my-extra-data", ctx,
      (GDestroyNotify) g_free);

  /* install the callback that will be called when a buffer is needed */
  g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
  gst_object_unref (appsrc);
  gst_object_unref (element);

}


int main(int argc, char *argv[])
{
	ctrl_window ctrl;
#ifdef GDK_WINDOWING_X11
	XInitThreads();
#endif
#if !GLIB_CHECK_VERSION(2, 31, 0)
	g_thread_init(NULL);
#endif
	gst_init(&argc, &argv);
	gtk_init(&argc, &argv);

	//create widgets
	ctrl.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	ctrl.boxmain = gtk_vbox_new(FALSE, 0);
	ctrl.boxgpi = gtk_hbox_new(TRUE, 0);
	ctrl.boxx = gtk_hbox_new(FALSE, 0);
	ctrl.boxy = gtk_hbox_new(FALSE, 0);
	ctrl.labelexp = gtk_label_new("Exposure (ms)");
	ctrl.labelgain = gtk_label_new("Gain (dB)");
	ctrl.labelx0 = gtk_label_new("x0");
	ctrl.labelcx = gtk_label_new("cx");
	ctrl.labely0 = gtk_label_new("y0");
	ctrl.labelcy = gtk_label_new("cy");
	ctrl.gpi1 = gtk_check_button_new_with_label("GPI1");
	ctrl.gpi2 = gtk_check_button_new_with_label("GPI2");
	ctrl.gpi3 = gtk_check_button_new_with_label("GPI3");
	ctrl.gpi4 = gtk_check_button_new_with_label("GPI4");
	ctrl.exp = gtk_hscale_new_with_range(1, 1000, 1);
	ctrl.gain = gtk_hscale_new_with_range(0, 1, 0.1); //use dummy limits
	ctrl.x0 = gtk_spin_button_new_with_range(0, 128, 2); //use dummy max limit
	ctrl.y0 = gtk_spin_button_new_with_range(0, 128, 2); //use dummy max limit
	ctrl.cx = gtk_spin_button_new_with_range(4, 128, 4); //use dummy max limit
	ctrl.cy = gtk_spin_button_new_with_range(2, 128, 2); //use dummy max limit
	ctrl.raw = gtk_toggle_button_new_with_label("Display RAW data");
	ctrl.show = gtk_toggle_button_new_with_label("Live view");
	ctrl.run = gtk_toggle_button_new_with_label("Acquisition");
	//tune them
	gtk_window_set_title(GTK_WINDOW(ctrl.window), "streamViewer control");
	gtk_window_set_keep_above(GTK_WINDOW(ctrl.window), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl.raw), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl.show), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl.run), TRUE); //actual start is delayed by 100ms, see below
	gtk_widget_set_sensitive(ctrl.boxgpi, FALSE);
	gtk_scale_set_digits(GTK_SCALE(ctrl.exp), 0);
	gtk_range_set_update_policy(GTK_RANGE(ctrl.exp), GTK_UPDATE_DISCONTINUOUS);
	gtk_range_set_update_policy(GTK_RANGE(ctrl.gain), GTK_UPDATE_DISCONTINUOUS);
	gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(ctrl.exp)), 10);
	gtk_scale_set_value_pos(GTK_SCALE(ctrl.exp), GTK_POS_RIGHT);
	gtk_scale_set_value_pos(GTK_SCALE(ctrl.gain), GTK_POS_RIGHT);
	gtk_widget_set_sensitive(ctrl.boxx, FALSE);
	gtk_widget_set_sensitive(ctrl.boxy, FALSE);
	gtk_widget_set_sensitive(ctrl.exp, FALSE);
	gtk_widget_set_sensitive(ctrl.gain, FALSE);
	//pack everything into window
	gtk_container_add(GTK_CONTAINER(ctrl.boxgpi), ctrl.gpi1);
	gtk_container_add(GTK_CONTAINER(ctrl.boxgpi), ctrl.gpi2);
	gtk_container_add(GTK_CONTAINER(ctrl.boxgpi), ctrl.gpi3);
	gtk_container_add(GTK_CONTAINER(ctrl.boxgpi), ctrl.gpi4);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.boxgpi);
	gtk_container_add(GTK_CONTAINER(ctrl.boxx), ctrl.labelx0);
	gtk_container_add(GTK_CONTAINER(ctrl.boxx), ctrl.x0);
	gtk_container_add(GTK_CONTAINER(ctrl.boxy), ctrl.labely0);
	gtk_container_add(GTK_CONTAINER(ctrl.boxy), ctrl.y0);
	gtk_container_add(GTK_CONTAINER(ctrl.boxx), ctrl.labelcx);
	gtk_container_add(GTK_CONTAINER(ctrl.boxx), ctrl.cx);
	gtk_container_add(GTK_CONTAINER(ctrl.boxy), ctrl.labelcy);
	gtk_container_add(GTK_CONTAINER(ctrl.boxy), ctrl.cy);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.boxx);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.boxy);	
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.labelexp);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.exp);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.labelgain);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.gain);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.raw);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.show);
	gtk_container_add(GTK_CONTAINER(ctrl.boxmain), ctrl.run);
	gtk_container_add(GTK_CONTAINER(ctrl.window), ctrl.boxmain);
	//register handlers
	gdk_threads_add_timeout(1000, (GSourceFunc)time_handler, (gpointer)&ctrl);
	gdk_threads_add_timeout(100, (GSourceFunc)start_cb, (gpointer)&ctrl); //only way I found to make sure window is displayed right away
	g_signal_connect(ctrl.window, "delete_event", G_CALLBACK(close_cb), (gpointer)TRUE);
	g_signal_connect(gtk_range_get_adjustment(GTK_RANGE(ctrl.gain)), "value_changed", G_CALLBACK(update_gain), NULL);
	g_signal_connect(gtk_range_get_adjustment(GTK_RANGE(ctrl.exp)), "value_changed", G_CALLBACK(update_exposure), NULL);
	g_signal_connect(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl.x0)), "value_changed", G_CALLBACK(update_x0), NULL);
	g_signal_connect(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl.y0)), "value_changed", G_CALLBACK(update_y0), NULL);
	g_signal_connect(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl.cx)), "value_changed", G_CALLBACK(update_cx), NULL);
	g_signal_connect(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(ctrl.cy)), "value_changed", G_CALLBACK(update_cy), NULL);
	g_signal_connect(ctrl.raw, "toggled", G_CALLBACK(update_raw), (gpointer)&ctrl);
	g_signal_connect(ctrl.show, "toggled", G_CALLBACK(update_show), NULL);
	g_signal_connect(ctrl.run, "toggled", G_CALLBACK(update_run), (gpointer)&ctrl);


//show window
	gtk_widget_show_all(ctrl.window);
	//start the main loop
  GMainLoop *loop;
  GstRTSPServer *server;
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;

  loop = g_main_loop_new (NULL, FALSE);

  /* create a server instance */
  server = gst_rtsp_server_new ();

  /* get the mount points for this server, every server has a default object
   * that be used to map uri mount points to media factories */
  mounts = gst_rtsp_server_get_mount_points (server);

  /* make a media factory for a test stream. The default media factory can use
   * gst-launch syntax to create pipelines.
   * any launch line works as long as it contains elements named pay%d. Each
   * element with pay%d names will be a stream */
  factory = gst_rtsp_media_factory_new ();

  //syd_ commandline
	gst_rtsp_media_factory_set_launch (factory,"("
				"appsrc name=mysrc ! slicer name=f "
				"f.src_0 ! videoconvert ! omxvp8enc name=enc0 ! rtpvp8pay name=pay0 pt=96 "
				"f.src_1 ! videoconvert ! omxvp8enc name=enc1 ! rtpvp8pay name=pay1 pt=97 "
				"f.src_2 ! videoconvert ! omxvp8enc name=enc2 ! rtpvp8pay name=pay2 pt=98 "
				"f.src_3 ! videoconvert ! omxvp8enc name=enc3 ! rtpvp8pay name=pay3 pt=99 "
				"f.src_4 ! videoconvert ! omxvp8enc name=enc4 ! rtpvp8pay name=pay4 pt=100 "
				"f.src_5 ! videoconvert ! omxvp8enc name=enc5 ! rtpvp8pay name=pay5 pt=101 "
				")");

  gst_rtsp_media_factory_set_shared (factory, TRUE);
  
  /* notify when our media is ready, This is called whenever someone asks for
   * the media and a new pipeline with our appsrc is created */
  g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);

  /* attach the test factory to the /test url */
  gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

  /* don't need the ref to the mounts anymore */
  g_object_unref (mounts);

  /* attach the server to the default maincontext */
  gst_rtsp_server_attach (server, NULL);

  /* start serving */
  g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
  g_main_loop_run (loop);


	return 0;
}
