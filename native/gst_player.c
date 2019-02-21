#include <gst/gstelement.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "gst_player.h"

// Object members
struct _RctGstPlayer {
    GObject  __unused parent_instance;

    gchar *debug_tag;
    gchar *parse_launch_pipeline;
    gpointer drawable_surface;

    GThread *thread;
    GMainLoop *loop;
    GstPipeline *pipeline;

    // States
    GstState desired_state;

    // Callbacks
    void (*on_rct_gst_player_loaded)(RctGstPlayer *self);

    void
    (*on_rct_gst_pipeline_state_changed)(RctGstPlayer *self, GstState new_state, GstState old_state);

    void (*on_rct_gst_pipeline_eos)(RctGstPlayer *self);

    void (*on_rct_gst_pipeline_error)(RctGstPlayer *self,
                                  const gchar *source,
                                  const gchar *message,
                                  const gchar *debug_info);

    void (*on_rct_gst_element_message)(RctGstPlayer *self, const gchar *element_name,
                                   const gchar *message_details);

    gpointer user_data;
} __unused;

G_DEFINE_TYPE(RctGstPlayer, rct_gst_player, G_TYPE_OBJECT)

// Globals static methods
static void rct_gst_player_set_debug_tag(RctGstPlayer *self, gchar *parse_launch_pipeline);

static void rct_gst_player_set_drawable_surface(RctGstPlayer *self, gpointer drawable_surface);

static void rct_gst_player_set_desired_state(RctGstPlayer *self, GstState state);

static void rct_gst_player_set_parse_launch_pipeline(RctGstPlayer *self,
                                                     gchar *parse_launch_pipeline);

static void cb_error(GstBus *bus, GstMessage *msg, RctGstPlayer *self) {
    (void) bus;
    (void) self;

    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    g_print("%s : Error received from element '%s' : %s\n",
              self->debug_tag,
              GST_OBJECT_NAME(msg->src),
              err->message);

    if (self->on_rct_gst_pipeline_error)
        self->on_rct_gst_pipeline_error(self,
                                    GST_OBJECT_NAME(msg->src),
                                    err->message,
                                    debug_info);

    g_clear_error(&err);
    g_free(debug_info);
}

static void cb_eos(GstBus *bus, GstMessage *msg, RctGstPlayer *self) {
    (void) bus;
    (void) msg;
    (void) self;

    g_print("%s : EOS\n", self->debug_tag);

    if (self->on_rct_gst_pipeline_eos)
        self->on_rct_gst_pipeline_eos(self);
}

static void cb_state_changed(GstBus *bus, GstMessage *message, RctGstPlayer *self) {
    (void) bus;

    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);

    g_print("%s : New state from '%s' : %s -> %s (%s pending)\n",
           self->debug_tag,
           GST_ELEMENT_NAME(GST_MESSAGE_SRC(message)),
           gst_element_state_get_name(old_state),
           gst_element_state_get_name(new_state),
           gst_element_state_get_name(pending_state));

    if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->pipeline)) {
        if (new_state > GST_STATE_READY)
            rct_gst_player_set_drawable_surface(self, self->drawable_surface);

        if (self->on_rct_gst_pipeline_state_changed)
            self->on_rct_gst_pipeline_state_changed(self, new_state, old_state);
    }
}

static void
rct_gst_player_serialize_value(JsonBuilder *builder, const gchar *field_name, const GValue *value) {
    GType field_type;

    field_type = G_VALUE_TYPE(value);

    if (field_name) // None when adding array entry
        json_builder_set_member_name(builder, field_name);

    if (field_type == G_TYPE_NONE) {
        json_builder_add_null_value(builder);

    } else if (field_type == G_TYPE_INT) {
        json_builder_add_int_value(builder, g_value_get_int(value));

    } else if (field_type == G_TYPE_INT64) {
        json_builder_add_int_value(builder, g_value_get_int64(value));

    } else if (field_type == G_TYPE_UINT) {
        json_builder_add_int_value(builder, g_value_get_uint(value));

    } else if (field_type == G_TYPE_UINT64) {
        json_builder_add_int_value(builder, g_value_get_uint64(value));

    } else if (field_type == G_TYPE_FLOAT) {
        json_builder_add_double_value(builder, g_value_get_float(value));

    } else if (field_type == G_TYPE_DOUBLE) {
        json_builder_add_double_value(builder, g_value_get_double(value));

    } else if (field_type == G_TYPE_BOOLEAN) {
        json_builder_add_boolean_value(builder, g_value_get_boolean(value));

    } else if (field_type == G_TYPE_STRING) {
        json_builder_add_string_value(builder, g_value_get_string(value));

    } else if (G_TYPE_IS_ENUM(field_type)) {
        json_builder_add_int_value(builder, g_value_get_enum(value));

    } else if (field_type == G_TYPE_VALUE_ARRAY) {
        GValueArray *value_array = NULL;
        uint i = 0;

        value_array = g_value_get_boxed(value);

        json_builder_begin_array(builder);
        for (i = 0; i < value_array->n_values; i++) {
            GValue subvalue = value_array->values[i];
            rct_gst_player_serialize_value(builder, NULL, &subvalue);
        }
        json_builder_end_array(builder);

    } else if (field_type == G_TYPE_ARRAY) {
        GArray *value_array = NULL;
        GValue *array_value = NULL;
        uint i = 0;

        value_array = g_value_get_boxed(value);

        json_builder_begin_array(builder);
        for (i = 0; i < value_array->len; i++) {
            array_value = &g_array_index(value_array, GValue, i);
            rct_gst_player_serialize_value(builder, NULL, array_value);
        }
        json_builder_end_array(builder);
    }
}

static gboolean cb_foreach_element_message_item(GQuark field_id,
                                                const GValue *value,
                                                gpointer user_data) {
    JsonBuilder *builder = NULL;
    const gchar *field_name = NULL;

    builder = (JsonBuilder *) user_data;
    field_name = g_quark_to_string(field_id);

    rct_gst_player_serialize_value(builder, field_name, value);

    return TRUE;
}

static gboolean cb_message_element(GstBus *bus, GstMessage *message, RctGstPlayer *self) {
    (void) bus;
    (void) message;
    (void) self;

    if (message->type == GST_MESSAGE_ELEMENT) {
        JsonBuilder *builder = NULL;
        JsonGenerator *gen = NULL;
        JsonNode *root = NULL;
        gchar *message_details = NULL;

        const GstStructure *message_structure = gst_message_get_structure(message);
        const gchar *name = gst_structure_get_name(message_structure);

        builder = json_builder_new();
        json_builder_begin_object(builder);

        gst_structure_foreach(message_structure, cb_foreach_element_message_item,
                              (gpointer) builder);

        json_builder_end_object(builder);

        gen = json_generator_new();
        root = json_builder_get_root(builder);
        json_generator_set_root(gen, root);
        message_details = json_generator_to_data(gen, NULL);

        json_node_free(root);
        g_object_unref(gen);
        g_object_unref(builder);

        
        if (message_details != NULL) {
            if (self->on_rct_gst_element_message)
                self->on_rct_gst_element_message(self, name, message_details);

            g_free(message_details);
        }
    }

    return TRUE;
}

static gboolean cb_async_done(GstBus *bus, GstMessage *message, RctGstPlayer *self) {
    (void) bus;
    (void) message;
    (void) self;

    return TRUE;
}

// Static callbacks
static gboolean cb_bus_watch(GstBus *bus, GstMessage *message, gpointer user_data) {
    RctGstPlayer *self = NULL;

    self = (RctGstPlayer *) user_data;

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR:
            cb_error(bus, message, self);
            break;

        case GST_MESSAGE_EOS:
            cb_eos(bus, message, self);
            break;

        case GST_MESSAGE_STATE_CHANGED:
            cb_state_changed(bus, message, self);
            break;

        case GST_MESSAGE_ELEMENT:
            cb_message_element(bus, message, self);
            break;

        case GST_MESSAGE_ASYNC_DONE:
            cb_async_done(bus, message, self);
            break;

        default:
            break;
    }

    return TRUE;
}

// Object methods
RctGstPlayer *rct_gst_player_new(const gchar *debug_tag,
                                 const gpointer cb_on_rct_gst_player_loaded,
                                 const gpointer cb_on_rct_gst_pipeline_state_changed,
                                 const gpointer cb_on_rct_gst_pipeline_eos,
                                 const gpointer cb_on_rct_gst_pipeline_error,
                                 const gpointer cb_on_rct_gst_element_message,
                                 const gpointer user_data) {

    RctGstPlayer *self = g_object_new(RCT_GST_TYPE_PLAYER,
                                      "debug_tag", debug_tag,
                                      "on_rct_gst_player_loaded", cb_on_rct_gst_player_loaded,
                                      "on_rct_gst_pipeline_state_changed", cb_on_rct_gst_pipeline_state_changed,
                                      "on_rct_gst_pipeline_eos", cb_on_rct_gst_pipeline_eos,
                                      "on_rct_gst_pipeline_error", cb_on_rct_gst_pipeline_error,
                                      "on_rct_gst_element_message", cb_on_rct_gst_element_message,
                                      "user_data", user_data,
                                      NULL);

    return self;
}

gpointer rct_gst_player_get_user_data(RctGstPlayer *self) {
    return self->user_data;
}

// Threading operations
static gpointer rct_gst_player_run_thread(gpointer data) {
    RctGstPlayer *self;

    self = (RctGstPlayer *) data;
    self->loop = g_main_loop_new(NULL, FALSE);

    g_print("%s : Player loop is starting...\n", self->debug_tag);

    if (self->on_rct_gst_player_loaded)
        self->on_rct_gst_player_loaded(self);

    g_main_loop_run(self->loop);
    g_print("%s : Player loop is stopping...\n", self->debug_tag);

    return data;
}

// Thread public starting point
void rct_gst_player_start(RctGstPlayer *self) {
    self->thread = g_thread_new("player_thread", rct_gst_player_run_thread, self);
}

void rct_gst_player_stop(RctGstPlayer *self) {
    g_main_loop_quit(self->loop);
}

// Setters
static void rct_gst_player_set_debug_tag(RctGstPlayer *self, gchar *parse_launch_pipeline) {
    self->debug_tag = parse_launch_pipeline;
    g_print("%s : Setting property debug_tag: %s\n", self->debug_tag, self->debug_tag);
}

static void rct_gst_player_set_drawable_surface(RctGstPlayer *self, gpointer drawable_surface) {
    GstElement *video_sink = NULL;
    GstVideoOverlay *video_overlay = NULL;

    self->drawable_surface = drawable_surface;
    g_print("%s : Setting property drawable_surface: %p\n", self->debug_tag,
           self->drawable_surface);

    video_sink = gst_bin_get_by_interface(GST_BIN(self->pipeline), GST_TYPE_VIDEO_OVERLAY);
    video_overlay = GST_VIDEO_OVERLAY(video_sink);

    if (video_overlay)
        gst_video_overlay_set_window_handle(video_overlay, (guintptr) self->drawable_surface);
}

static void rct_gst_player_set_desired_state(RctGstPlayer *self, GstState state) {
    g_print("%s : Setting pipeline state: %s\n", self->debug_tag,
           gst_element_state_get_name(state));

    gst_element_set_state(GST_ELEMENT(self->pipeline), state);
}

static void rct_gst_player_set_parse_launch_pipeline(RctGstPlayer *self,
                                                     gchar *parse_launch_pipeline) {
    GstBus *bus;

    if (self->pipeline) {
        g_print("%s : Cleaning old pipeline: %p\n", self->debug_tag,
               self->pipeline);

        GstBus *current_bus;

        current_bus = gst_pipeline_get_bus(self->pipeline);
        gst_bus_remove_watch(current_bus);

        gst_element_set_state(GST_ELEMENT(self->pipeline), GST_STATE_NULL);

        gst_object_unref(current_bus);

        gst_object_unref(self->pipeline);
        self->pipeline = NULL;
    }

    g_print("%s : Creating new pipeline\n", self->debug_tag);

    self->parse_launch_pipeline = parse_launch_pipeline;
    g_print("%s : Setting property parse_launch_pipeline: %s\n", self->debug_tag,
           self->parse_launch_pipeline);

    self->pipeline = GST_PIPELINE(gst_parse_launch(self->parse_launch_pipeline, NULL));
    bus = gst_pipeline_get_bus(self->pipeline);

    gst_bus_add_watch(bus, cb_bus_watch, (gpointer) self);

    rct_gst_player_set_desired_state(self, self->desired_state);

    gst_object_unref(bus);
}

static GstElement *rct_gst_player_get_element(RctGstPlayer *self,
                                              const gchar *element_name) {

    GstElement *element = NULL;

    element = gst_bin_get_by_name(GST_BIN (self->pipeline), element_name);
    if (element == NULL) {
        gchar *error_detail = g_strdup_printf("Element %s doesn't exists", element_name);

        if (self->on_rct_gst_pipeline_error)
            self->on_rct_gst_pipeline_error(self, "pipeline", error_detail, "");

        g_free(error_detail);
        return NULL;
    }

    return element;
}

static void rct_gst_player_set_element_property(RctGstPlayer *self,
                                                const gchar *element_name,
                                                const gchar *property_name,
                                                JsonNode *property_node) {
    GstElement *element = NULL;
    GValue value = G_VALUE_INIT;

    element = rct_gst_player_get_element(self, element_name);
    if (element == NULL)
        return;

    json_node_get_value(property_node, &value);
    g_object_set_property(G_OBJECT(element), property_name, &value);
    g_value_unset(&value);
}

void rct_gst_player_lookup_properties(RctGstPlayer *self,
                                      JsonNode *node,
                                      const gchar *element_name,
                                      const gchar *property_name) {
    GType node_type = json_node_get_value_type(node);

    if (element_name != NULL && property_name != NULL) {
        rct_gst_player_set_element_property(self, element_name, property_name, node);
    }

    if (node_type == JSON_TYPE_OBJECT) {
        JsonObject *object = NULL;
        JsonObjectIter object_iterator;
        const gchar *member_name;
        JsonNode *member_node = NULL;

        object = json_node_get_object(node);
        json_object_iter_init(&object_iterator, object);

        while (json_object_iter_next(&object_iterator, &member_name, &member_node)) {
            if (element_name != NULL)
                rct_gst_player_lookup_properties(self, member_node, element_name, member_name);
            else
                rct_gst_player_lookup_properties(self, member_node, member_name, NULL);
        }
    }
}

void rct_gst_player_set_pipeline_properties(RctGstPlayer *self, const gchar *pipeline_properties) {

    JsonParser *parser = NULL;
    GError *error = NULL;
    JsonNode *elements_node = NULL;

    parser = json_parser_new();
    json_parser_load_from_data(parser, pipeline_properties, -1, &error);
    if (error != NULL) {
        g_print("%s : Unable to parse '%s': %s\n", self->debug_tag, pipeline_properties,
                  error->message);

        g_error_free(error);
        g_object_unref(parser);
        return;
    }

    elements_node = json_parser_get_root(parser);
    rct_gst_player_lookup_properties(self, elements_node, NULL, NULL);
}

// Object properties
enum {
    PROP_DEBUG_TAG = 1,
    PROP_PARSE_LAUNCH_PIPELINE_TAG,
    PROP_DRAWABLE_SURFACE_TAG,
    PROP_CB_ON_RCT_GST_PLAYER_LOADED_TAG,
    PROP_CB_ON_RCT_GST_PIPELINE_STATE_CHANGED_TAG,
    PROP_CB_ON_RCT_GST_PIPELINE_EOS_TAG,
    PROP_CB_ON_RCT_GST_PIPELINE_ERROR_TAG,
    PROP_CB_ON_RCT_GST_ELEMENT_MESSAGE_TAG,
    PROP_USER_DATA_TAG,
    PROP_DESIRED_STATE_TAG,
    N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = {
        NULL,
};

// Lifecycle methods
static void
rct_gst_player_set_property(GObject *object, guint property_id, const GValue *value,
                            GParamSpec *pspec) {
    RctGstPlayer *self = RCT_GST_PLAYER(object);

    switch (property_id) {
        case PROP_DEBUG_TAG:
            g_free(self->debug_tag);
            rct_gst_player_set_debug_tag(self, g_value_dup_string(value));
            break;

        case PROP_PARSE_LAUNCH_PIPELINE_TAG:
            g_free(self->parse_launch_pipeline);
            rct_gst_player_set_parse_launch_pipeline(self, g_value_dup_string(value));
            break;

        case PROP_DRAWABLE_SURFACE_TAG:
            rct_gst_player_set_drawable_surface(self, g_value_get_pointer(value));
            break;

        case PROP_CB_ON_RCT_GST_PLAYER_LOADED_TAG:
            self->on_rct_gst_player_loaded = g_value_get_pointer(value);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_STATE_CHANGED_TAG:
            self->on_rct_gst_pipeline_state_changed = g_value_get_pointer(value);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_EOS_TAG:
            self->on_rct_gst_pipeline_eos = g_value_get_pointer(value);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_ERROR_TAG:
            self->on_rct_gst_pipeline_error = g_value_get_pointer(value);
            break;

        case PROP_CB_ON_RCT_GST_ELEMENT_MESSAGE_TAG:
            self->on_rct_gst_element_message = g_value_get_pointer(value);
            break;

        case PROP_USER_DATA_TAG:
            self->user_data = g_value_get_pointer(value);
            break;

        case PROP_DESIRED_STATE_TAG:
            self->desired_state = (GstState) g_value_get_int(value);
            rct_gst_player_set_desired_state(self, self->desired_state);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
rct_gst_player_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    RctGstPlayer *self = RCT_GST_PLAYER(object);

    switch (property_id) {
        case PROP_DEBUG_TAG:
            g_value_set_string(value, self->debug_tag);
            break;

        case PROP_PARSE_LAUNCH_PIPELINE_TAG:
            g_value_set_string(value, self->parse_launch_pipeline);
            break;

        case PROP_DRAWABLE_SURFACE_TAG:
            g_value_set_pointer(value, self->drawable_surface);
            break;

        case PROP_CB_ON_RCT_GST_PLAYER_LOADED_TAG:
            g_value_set_pointer(value, self->on_rct_gst_player_loaded);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_STATE_CHANGED_TAG:
            g_value_set_pointer(value, self->on_rct_gst_pipeline_state_changed);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_EOS_TAG:
            g_value_set_pointer(value, self->on_rct_gst_pipeline_eos);
            break;

        case PROP_CB_ON_RCT_GST_PIPELINE_ERROR_TAG:
            g_value_set_pointer(value, self->on_rct_gst_pipeline_error);
            break;

        case PROP_CB_ON_RCT_GST_ELEMENT_MESSAGE_TAG:
            g_value_set_pointer(value, self->on_rct_gst_element_message);
            break;

        case PROP_USER_DATA_TAG:
            g_value_set_pointer(value, self->user_data);
            break;

        case PROP_DESIRED_STATE_TAG:
            g_value_set_int(value, (int) self->desired_state);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void rct_gst_player_dispose(GObject *object) {
    RctGstPlayer *self = RCT_GST_PLAYER(object);

    g_print("%s : Disposing Gst Player...", self->debug_tag);
}

static void rct_gst_player_finalize(GObject *object) {
    RctGstPlayer *self = RCT_GST_PLAYER(object);

    g_print("%s : Finalizing Gst Player...", self->debug_tag);
    g_free(self->debug_tag);
    g_free(self->parse_launch_pipeline);
    g_main_loop_unref(self->loop);

    g_thread_unref(self->thread);

    self->debug_tag = NULL;
    self->parse_launch_pipeline = NULL;
    self->loop = NULL;
}

static void __unused rct_gst_player_class_init(RctGstPlayerClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = rct_gst_player_set_property;
    object_class->get_property = rct_gst_player_get_property;
    object_class->dispose = rct_gst_player_dispose;
    object_class->finalize = rct_gst_player_finalize;

    obj_properties[PROP_DEBUG_TAG] =
            g_param_spec_string("debug_tag",
                                "Debug Tag",
                                "Display tag for printing facilities.",
                                "RCTGstPlayer (Native Code)",
                                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_PARSE_LAUNCH_PIPELINE_TAG] =
            g_param_spec_string("parse_launch_pipeline",
                                "Parse Launch Pipeline",
                                "Content for gst_parse_launch command.",
                                "",
                                G_PARAM_READWRITE);

    obj_properties[PROP_DRAWABLE_SURFACE_TAG] =
            g_param_spec_pointer("drawable_surface",
                                 "Drawable Surface",
                                 "Pointer to a drawable surface",
                                 G_PARAM_READWRITE);

    obj_properties[PROP_CB_ON_RCT_GST_PLAYER_LOADED_TAG] =
            g_param_spec_pointer("on_rct_gst_player_loaded",
                                 "On GST Player loaded",
                                 "Callback which will be called when the player is ready to be used",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_CB_ON_RCT_GST_PIPELINE_STATE_CHANGED_TAG] =
            g_param_spec_pointer("on_rct_gst_pipeline_state_changed",
                                 "On GST Pipeline state changed",
                                 "Callback which will be called when the player changes its pipeline state",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_CB_ON_RCT_GST_PIPELINE_EOS_TAG] =
            g_param_spec_pointer("on_rct_gst_pipeline_eos",
                                 "On GST Pipeline End Of Stream",
                                 "Callback which will be called when the player has done playing media",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_CB_ON_RCT_GST_PIPELINE_ERROR_TAG] =
            g_param_spec_pointer("on_rct_gst_pipeline_error",
                                 "On GST Pipeline Error",
                                 "Callback which will be called when the player has met an error",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_CB_ON_RCT_GST_ELEMENT_MESSAGE_TAG] =
            g_param_spec_pointer("on_rct_gst_element_message",
                                 "On GST Element Message",
                                 "Callback which will be called when an element get a message",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_USER_DATA_TAG] =
            g_param_spec_pointer("user_data",
                                 "User Data",
                                 "Contains context specifics data",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[PROP_DESIRED_STATE_TAG] =
            g_param_spec_int("desired_state",
                             "Desired State",
                             "Defines the targeted gstreamer state",
                             GST_STATE_VOID_PENDING,
                             GST_STATE_PLAYING,
                             GST_STATE_VOID_PENDING,
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      N_PROPERTIES,
                                      obj_properties);
}

static void __unused rct_gst_player_init(RctGstPlayer *self) {
    self->parse_launch_pipeline = NULL;
    self->drawable_surface = NULL;
    self->desired_state = GST_STATE_VOID_PENDING;
    self->loop = NULL;
    self->user_data = NULL;
}
