package com.asap.reactnativegstplayer;

import android.util.Log;
import android.widget.Toast;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.ReadableNativeMap;
import com.facebook.react.common.MapBuilder;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

import org.freedesktop.gstreamer.GStreamer;

import java.util.Map;

import javax.annotation.Nullable;

public class GstPlayerManager extends SimpleViewManager<GstPlayerView> {

    private static int RCT_GST_PLAYER_INDEX = 0;
    private static final String REACT_CLASS = "RCTGstPlayer";
    private static boolean GSTREAMER_INITIALIZED = false;

    private native String jniGetGStreamerVersion();

    @Override
    public String getName() {
        return GstPlayerManager.REACT_CLASS;
    }

    @Override
    protected GstPlayerView createViewInstance(ThemedReactContext reactContext) {
        if (!GSTREAMER_INITIALIZED) {
            try {
                GStreamer.init(reactContext);
                Log.i(REACT_CLASS, String.format("Initialized %s", this.jniGetGStreamerVersion()));
                GSTREAMER_INITIALIZED = true;
            } catch (Exception e) {
                Toast.makeText(reactContext, e.getMessage(), Toast.LENGTH_LONG).show();
                Log.e(REACT_CLASS, e.getLocalizedMessage());
            }
        }

        GstPlayerViewController viewController = new GstPlayerViewController(
                ++RCT_GST_PLAYER_INDEX,
                reactContext
        );

        reactContext.addLifecycleEventListener(viewController);

        return viewController.createView();
    }

    @ReactProp(name = "parseLaunchPipeline")
    public void setParseLaunchPipeline(GstPlayerView view, String url) {
        view.getController().setParseLaunchPipeline(url);
    }

    @ReactProp(name = "pipelineState")
    public void setPipelineState(GstPlayerView view, int pipelineState) {
        view.getController().setPipelineState(pipelineState);
    }

    @ReactProp(name = "properties")
    public void setPipelineProperties(GstPlayerView view, String pipelineProperties) {
        view.getController().setPipelineProperties(pipelineProperties);
    }

    @Nullable
    @Override
    public Map<String, Object> getExportedCustomDirectEventTypeConstants() {
        return MapBuilder.<String, Object>builder()
                .put(
                        "onGstPipelineStateChanged", MapBuilder.of("registrationName", "onGstPipelineStateChanged")
                ).put(
                        "onGstPipelineEOS", MapBuilder.of("registrationName", "onGstPipelineEOS")
                ).put(
                        "onGstPipelineError", MapBuilder.of("registrationName", "onGstPipelineError")
                ).put(
                        "onGstElementMessage", MapBuilder.of("registrationName", "onGstElementMessage")
                ).build();
    }

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("rctgstplayer");
    }
}
