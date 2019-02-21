#include "gst_player.h"

static gchar *debug_tag = "Desktop Player";

static void cb_on_rct_gst_player_loaded(RctGstPlayer *rct_gst_player)
{
  g_info("%s - Player ready.\n", debug_tag);
}

static void cb_on_rct_gst_pipeline_state_changed(RctGstPlayer *rct_gst_player, GstState new_state, GstState old_state)
{
  g_info("%s - Player state changed from %s to %s\n", debug_tag, gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
}

static void cb_on_rct_gst_pipeline_eos(RctGstPlayer *rct_gst_player)
{
  g_info("%s - Player EOS\n", debug_tag);
}

static void cb_on_rct_gst_pipeline_error(RctGstPlayer *rct_gst_player, const gchar *source, const gchar *message, const gchar *debug_info)
{
  g_info("%s - Pipeline Error from '%s' : %s \n\tDebug informations : %s.\n", debug_tag, source, message, debug_info);
}

static void cb_on_rct_gst_element_message(RctGstPlayer *rct_gst_player, const gchar *element_name, const gchar *message)
{
  g_info("%s - Pipeline Element message from '%s' : %s\n", debug_tag, element_name, message);
}

static gboolean apply_properties(gpointer user_data)
{
  g_info("%s - Applying properties", debug_tag);
  RctGstPlayer *rct_gst_player = user_data;
  const gchar *properties = "{\"decodeBin\":{\"uri\":\"https://www.sample-videos.com/video123/mp4/720/big_buck_bunny_720p_1mb.mp4\"},\"videoSrc\":{\"pattern\":13,\"is-live\":false},\"videoSink\":{\"sync\":false,\"text-overlay\":true},\"audioSrc\":{\"freq\":440},\"volumeControl\":{\"volume\":0.75,\"mute\":false},\"textOverlay\":{\"\text\":\"Hello there\",\"font-desc\":\"Times, 25\",\"valignment\":2,\"halignment\":0},\"levelInfo\":{\"interval\":1000000000,\"post-messages\":true}}";
  rct_gst_player_set_pipeline_properties(rct_gst_player, properties);

  return FALSE;
}

static gpointer parallel_thread_start(gpointer user_data)
{
  g_info("%s - parallel_thread_start", debug_tag);
  g_timeout_add(3000, apply_properties, user_data);
}

int main(int argc, char **argv)
{
  gst_init(&argc, &argv);

  RctGstPlayer *rct_gst_player = rct_gst_player_new(debug_tag,
                                                    cb_on_rct_gst_player_loaded,
                                                    cb_on_rct_gst_pipeline_state_changed,
                                                    cb_on_rct_gst_pipeline_eos,
                                                    cb_on_rct_gst_pipeline_error,
                                                    cb_on_rct_gst_element_message,
                                                    NULL);

  rct_gst_player_start(rct_gst_player);

  g_object_set(rct_gst_player,
               "parse_launch_pipeline",
               "gltestsrc name=\"videoSrc\" ! queue ! video/x-raw(memory:GLMemory),width=1920,height=1080,framerate=30/1 ! fpsdisplaysink name=videoSink  audiotestsrc name=\"audioSrc\" ! queue ! volume name=volumeControl ! level name=\"levelInfo\" ! audioconvert ! audioresample ! autoaudiosink",
               NULL);

  g_object_set(rct_gst_player,
               "desired_state",
               GST_STATE_PLAYING,
               NULL);

  GThread *thread = g_thread_new("desktop controlling", parallel_thread_start, rct_gst_player);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(main_loop);

  return 0;
}