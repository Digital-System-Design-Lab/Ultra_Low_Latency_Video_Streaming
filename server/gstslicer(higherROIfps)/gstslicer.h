
/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_SLICER_H_
#define _GST_SLICER_H_

#include <gst/gst.h>
#include <gst/video/video.h>
G_BEGIN_DECLS

#define GST_TYPE_SLICER   (gst_slicer_get_type())
#define GST_SLICER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SLICER,GstSlicer))
#define GST_SLICER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SLICER,GstSlicerClass))
#define GST_SLICER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_SLICER,GstSlicerClass))
#define GST_IS_SLICER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SLICER))
#define GST_IS_SLICER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SLICER))

typedef struct _GstSlicer GstSlicer;
typedef struct _GstSlicerClass GstSlicerClass;

struct _GstSlicer
{
  GstElement element;

  GList *srcpads;
  GstCaps *sinkcaps;
  GstPad *sinkpad;

  GstVideoInfo video_info;

  GList *pending_events;

};

struct _GstSlicerClass
{
  GstElementClass base_slicer_class;
};

GType gst_slicer_get_type (void);

G_END_DECLS

#endif
