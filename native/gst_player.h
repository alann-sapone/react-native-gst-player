#ifndef __GST_PLAYER_FILE_H__
#define __GST_PLAYER_FILE_H__

#include <glib-object.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

// Type declaration
#define RCT_GST_TYPE_PLAYER rct_gst_player_get_type ()

G_DECLARE_FINAL_TYPE (RctGstPlayer, rct_gst_player, RCT_GST, PLAYER, GObject)

__unused

// Methods definitions
RctGstPlayer *rct_gst_player_new(const gchar *debug_tag,
                              const gpointer cb_on_rct_gst_player_loaded,
                              const gpointer cb_on_rct_gst_pipeline_state_changed,
                              const gpointer cb_on_rct_gst_pipeline_eos,
                              const gpointer cb_on_rct_gst_pipeline_error,
                              const gpointer cb_on_rct_gst_element_message,
                              const gpointer user_data);

void rct_gst_player_start(RctGstPlayer *self); // Prepares and runs the player in a thread
void rct_gst_player_set_pipeline_properties(RctGstPlayer *self, const gchar *pipeline_properties);

void rct_gst_player_stop(RctGstPlayer *self);

gpointer rct_gst_player_get_user_data(RctGstPlayer *self);

G_END_DECLS

#endif /* __GST_PLAYER_FILE_H__ */
