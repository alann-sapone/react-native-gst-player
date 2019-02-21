#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <pthread.h>

#include "gst/gst.h"
#include "gst_player.h"
#include "android_user_data.h"

// JNI Specifics
static JavaVM *jvm;
static pthread_key_t current_jni_env;

static void detach_current_thread(void *env);

static JNIEnv *attach_current_thread(void);

static JNIEnv *get_jni_env(void);


/*
 * JNI RctGstPlayer Lifecycle
 */

// Java callbacks
static jmethodID on_rct_gst_player_loaded_method_id;
static jmethodID on_rct_gst_pipeline_state_changed_method_id;
static jmethodID on_rct_gst_pipeline_eos_method_id;
static jmethodID on_rct_gst_pipeline_error_method_id;
static jmethodID on_rct_gst_element_message_method_id;

static void cb_on_rct_gst_player_loaded(RctGstPlayer *rct_gst_player);
static void cb_on_rct_gst_pipeline_state_changed(RctGstPlayer *rct_gst_player, GstState new_state, GstState old_state);
static void cb_on_rct_gst_pipeline_eos(RctGstPlayer *rct_gst_player);
static void cb_on_rct_gst_pipeline_error(RctGstPlayer *rct_gst_player,
                                         const gchar *source,
                                         const gchar *message,
                                         const gchar *debug_info);
static void cb_on_rct_gst_element_message(RctGstPlayer *rct_gst_player,
                                          const gchar *element,
                                          const gchar *message);

// Returns a jstring containing an utf8 version of currently linked GStreamer
static jstring get_gstreamer_version(JNIEnv *env, jobject thiz) {
    (void) thiz;

    gchar *version_utf8 = NULL;
    jstring *version_jstring = NULL;

    version_utf8 = gst_version_string();
    version_jstring = (*env)->NewStringUTF(env, version_utf8);

    g_free(version_utf8);

    return version_jstring;
}

// Create a RctGstPlayer instance and returns it in the form of a DirectBuffer
static jobject create_rct_gst_player(JNIEnv *env, jobject thiz,
                                     jstring j_debug_tag) {
    (void) thiz;

    RctGstPlayer *rct_gst_player = NULL;
    const gchar *debug_tag;
    AndroidUserData *user_data;

    user_data = g_malloc0(sizeof(AndroidUserData));
    user_data->thiz = (*env)->NewGlobalRef(env, thiz);

    debug_tag = (*env)->GetStringUTFChars(env, j_debug_tag, NULL);
    rct_gst_player = rct_gst_player_new(debug_tag,
                                        cb_on_rct_gst_player_loaded,
                                        cb_on_rct_gst_pipeline_state_changed,
                                        cb_on_rct_gst_pipeline_eos,
                                        cb_on_rct_gst_pipeline_error,
                                        cb_on_rct_gst_element_message,
                                        (gpointer) user_data);

    (*env)->ReleaseStringUTFChars(env, j_debug_tag, debug_tag);

    rct_gst_player_start(rct_gst_player);

    return (*env)->NewDirectByteBuffer(env, (void *) rct_gst_player, sizeof(RctGstPlayer *));
}

// Ask to kill the RctGstPlayer instance
static void destroy_rct_gst_player(JNIEnv *env, jobject thiz,
                                   jobject j_rct_gst_player) {
    (void) thiz;

    RctGstPlayer *rct_gst_player = NULL;
    gpointer current_native_window = NULL;

    rct_gst_player = (RctGstPlayer *) (*env)->GetDirectBufferAddress(env, j_rct_gst_player);
    g_object_get(rct_gst_player, "drawable_surface", &current_native_window, NULL);
    g_object_set(rct_gst_player, "desired_state", GST_STATE_NULL, NULL);

    if (current_native_window)
        ANativeWindow_release(current_native_window);

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer", "Destroying player");
    rct_gst_player_stop(rct_gst_player);

    g_object_unref(rct_gst_player);
}

/*
 * JNI RctGstPlayer Methods binding
 */
static void set_parse_launch_pipeline(JNIEnv *env, jobject thiz,
                                      jobject j_rct_gst_player,
                                      jstring j_parse_launch_pipeline) {
    (void) thiz;

    RctGstPlayer *rct_gst_player = NULL;
    const gchar *parse_launch_pipeline;

    rct_gst_player = (RctGstPlayer *) (*env)->GetDirectBufferAddress(env, j_rct_gst_player);
    parse_launch_pipeline = (*env)->GetStringUTFChars(env, j_parse_launch_pipeline, NULL);
    g_object_set(rct_gst_player, "parse_launch_pipeline", parse_launch_pipeline, NULL);

    (*env)->ReleaseStringUTFChars(env, j_parse_launch_pipeline, parse_launch_pipeline);
}

static void set_drawable_surface(JNIEnv *env, jobject thiz,
                                 jobject j_rct_gst_player,
                                 jobject j_drawable_surface) {
    (void) thiz;

    RctGstPlayer *rct_gst_player = NULL;
    ANativeWindow *j_new_native_window = NULL;
    gpointer new_native_window = NULL;
    gpointer current_native_window = NULL;

    rct_gst_player = (RctGstPlayer *) (*env)->GetDirectBufferAddress(env, j_rct_gst_player);
    j_new_native_window = ANativeWindow_fromSurface(env, j_drawable_surface);
    new_native_window = (gpointer) j_new_native_window;
    g_object_get(rct_gst_player, "drawable_surface", &current_native_window, NULL);

    if (current_native_window) {
        ANativeWindow_release(current_native_window);

        if (current_native_window == new_native_window) {
            __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                                "New native window is the same as the previous one %p",
                                current_native_window);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                                "Released previous native window %p",
                                current_native_window);
        }
    }

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                        "Setting native window : %p",
                        new_native_window);

    g_object_set(rct_gst_player, "drawable_surface", new_native_window, NULL);
}

static void set_pipeline_state(JNIEnv *env, jobject thiz,
                               jobject j_rct_gst_player,
                               jint j_gst_state) {
    (void) thiz;
    RctGstPlayer *rct_gst_player = NULL;

    rct_gst_player = (RctGstPlayer *) (*env)->GetDirectBufferAddress(env, j_rct_gst_player);

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                        "Setting pipeline desired state to : %s",
                        gst_element_state_get_name((GstState) j_gst_state));

    g_object_set(rct_gst_player, "desired_state", j_gst_state, NULL);
}

static void set_pipeline_properties(JNIEnv *env, jobject thiz,
                                    jobject j_rct_gst_player,
                                    jstring j_pipeline_properties) {
    (void) thiz;

    RctGstPlayer *rct_gst_player = NULL;
    const gchar *pipeline_properties = (*env)->GetStringUTFChars(env, j_pipeline_properties, NULL);

    rct_gst_player = (RctGstPlayer *) (*env)->GetDirectBufferAddress(env, j_rct_gst_player);

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                        "Setting pipeline properties with json : %s", pipeline_properties);

    rct_gst_player_set_pipeline_properties(rct_gst_player, pipeline_properties);

    (*env)->ReleaseStringUTFChars(env, j_pipeline_properties, pipeline_properties);
}

// Java callbacks bindings
static void cb_on_rct_gst_player_loaded(RctGstPlayer *rct_gst_player) {
    JNIEnv *env = NULL;

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer", "Player is ready.");

    env = get_jni_env();

    AndroidUserData *user_data = (AndroidUserData *) rct_gst_player_get_user_data(rct_gst_player);
    (*env)->CallVoidMethod(env, user_data->thiz, on_rct_gst_player_loaded_method_id);
}

static void
cb_on_rct_gst_pipeline_state_changed(RctGstPlayer *rct_gst_player, GstState new_state, GstState old_state) {
    JNIEnv *env = NULL;

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer", "Player changed state : %s",
                        gst_element_state_get_name(new_state));

    env = get_jni_env();

    AndroidUserData *user_data = (AndroidUserData *) rct_gst_player_get_user_data(rct_gst_player);
    (*env)->CallVoidMethod(env, user_data->thiz, on_rct_gst_pipeline_state_changed_method_id,
                           (jint) new_state,
                           (jint) old_state);
}

static void cb_on_rct_gst_pipeline_eos(RctGstPlayer *rct_gst_player) {
    JNIEnv *env = NULL;

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer", "Player EOS.");

    env = get_jni_env();
    AndroidUserData *user_data = (AndroidUserData *) rct_gst_player_get_user_data(rct_gst_player);
    (*env)->CallVoidMethod(env, user_data->thiz, on_rct_gst_pipeline_eos_method_id);
}

static void cb_on_rct_gst_pipeline_error(RctGstPlayer *rct_gst_player,
                                         const gchar *source,
                                         const gchar *message,
                                         const gchar *debug_info) {
    JNIEnv *env = NULL;
    jstring j_source = NULL;
    jstring j_message = NULL;
    jstring j_debug_info = NULL;

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                        "Player Error from '%s' : %s\n\tDebug info : %s",
                        source,
                        message,
                        debug_info);

    env = get_jni_env();
    j_source = (*env)->NewStringUTF(env, source);
    j_message = (*env)->NewStringUTF(env, message);
    j_debug_info = (*env)->NewStringUTF(env, debug_info);

    AndroidUserData *user_data = (AndroidUserData *) rct_gst_player_get_user_data(rct_gst_player);
    (*env)->CallVoidMethod(env, user_data->thiz, on_rct_gst_pipeline_error_method_id,
                           j_source,
                           j_message,
                           j_debug_info);

    (*env)->DeleteLocalRef(env, j_source);
    (*env)->DeleteLocalRef(env, j_message);
    (*env)->DeleteLocalRef(env, j_debug_info);
}

static void cb_on_rct_gst_element_message(RctGstPlayer *rct_gst_player,
                                          const gchar *element_name,
                                          const gchar *message) {
    JNIEnv *env = NULL;
    jstring j_element_name = NULL;
    jstring j_message_details = NULL;

    __android_log_print(ANDROID_LOG_INFO, "JNI - RCTGstPlayer",
                        "Player element '%s' with message : %s", element_name, message);

    env = get_jni_env();
    j_element_name = (*env)->NewStringUTF(env, element_name);
    j_message_details = (*env)->NewStringUTF(env, message);

    AndroidUserData *user_data = (AndroidUserData *) rct_gst_player_get_user_data(rct_gst_player);
    (*env)->CallVoidMethod(env, user_data->thiz, on_rct_gst_element_message_method_id,
                           j_element_name,
                           j_message_details);

    (*env)->DeleteLocalRef(env, j_element_name);
    (*env)->DeleteLocalRef(env, j_message_details);
}

/*
 * JNI Native methods bindings
 */
static JNINativeMethod gst_player_controller_native_methods[] = {
        {"jniCreateGstPlayer",        "(Ljava/lang/String;)Ljava/nio/ByteBuffer;",      (void *) create_rct_gst_player},
        {"jniDestroyGstPlayer",       "(Ljava/nio/ByteBuffer;)V",                       (void *) destroy_rct_gst_player},
        {"jniSetParseLaunchPipeline", "(Ljava/nio/ByteBuffer;Ljava/lang/String;)V",     (void *) set_parse_launch_pipeline},
        {"jniSetDrawableSurface",     "(Ljava/nio/ByteBuffer;Landroid/view/Surface;)V", (void *) set_drawable_surface},
        {"jniSetPipelineState",       "(Ljava/nio/ByteBuffer;I)V",                      (void *) set_pipeline_state},
        {"jniSetPipelineProperties",  "(Ljava/nio/ByteBuffer;Ljava/lang/String;)V",     (void *) set_pipeline_properties},
};

static JNINativeMethod gst_player_manager_native_methods[] = {
        {"jniGetGStreamerVersion", "()Ljava/lang/String;", (void *) get_gstreamer_version}
};

/*
 * JNI specifics
 */

static void detach_current_thread(void *env) {
    (void) env;

    GST_DEBUG("Detaching thread %p", g_thread_self());
    (*jvm)->DetachCurrentThread(jvm);
}

static JNIEnv *attach_current_thread(void) {
    JNIEnv *env;
    JavaVMAttachArgs args;

    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;

    if ((*jvm)->AttachCurrentThread(jvm, &env, &args) < 0) {
        GST_ERROR("Failed to attach current thread");
        return NULL;
    }

    return env;
}

static JNIEnv *get_jni_env(void) {
    JNIEnv *env;

    if ((env = pthread_getspecific(current_jni_env)) == NULL) {
        env = attach_current_thread();
        pthread_setspecific(current_jni_env, env);
    }

    return env;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void) reserved;

    JNIEnv *env = NULL;
    jclass gst_player_controller_class = NULL;
    jclass gst_player_manager_class = NULL;

    jvm = vm;

    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "JNI - RCTGstPlayer", "Could not retrieve JNIEnv");
        return 0;
    }


    // GstPlayerController Java Class
    const gchar *gpc_classname = "com/asap/reactnativegstplayer/GstPlayerViewController";
    gst_player_controller_class = (*env)->FindClass(env, gpc_classname);
    (*env)->RegisterNatives(env,
                            gst_player_controller_class,
                            gst_player_controller_native_methods,
                            G_N_ELEMENTS(gst_player_controller_native_methods));


    // GstPlayerManager Java Class
    const gchar *gpm_classname = "com/asap/reactnativegstplayer/GstPlayerManager";
    gst_player_manager_class = (*env)->FindClass(env, gpm_classname);
    (*env)->RegisterNatives(env,
                            gst_player_manager_class,
                            gst_player_manager_native_methods,
                            G_N_ELEMENTS(gst_player_manager_native_methods));


    // Registering java methods
    on_rct_gst_player_loaded_method_id = (*env)->GetMethodID(env,
                                                         gst_player_controller_class,
                                                         "onGstPlayerLoaded",
                                                         "()V");

    on_rct_gst_pipeline_state_changed_method_id = (*env)->GetMethodID(env,
                                                                  gst_player_controller_class,
                                                                  "onGstPipelineStateChanged",
                                                                  "(II)V");

    on_rct_gst_pipeline_eos_method_id = (*env)->GetMethodID(env,
                                                        gst_player_controller_class,
                                                        "onGstPipelineEOS",
                                                        "()V");

    on_rct_gst_pipeline_error_method_id = (*env)->GetMethodID(env,
                                                          gst_player_controller_class,
                                                          "onGstPipelineError",
                                                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

    on_rct_gst_element_message_method_id = (*env)->GetMethodID(env,
                                                           gst_player_controller_class,
                                                           "onGstElementMessage",
                                                           "(Ljava/lang/String;Ljava/lang/String;)V");

    pthread_key_create(&current_jni_env, detach_current_thread);

    return JNI_VERSION_1_6;
}