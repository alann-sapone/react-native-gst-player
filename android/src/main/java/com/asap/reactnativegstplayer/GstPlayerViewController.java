package com.asap.reactnativegstplayer;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;

import com.asap.reactnativegstplayer.utils.GstState;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.LifecycleEventListener;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.events.RCTEventEmitter;

import java.nio.ByteBuffer;
import java.util.Locale;

public class GstPlayerViewController implements LifecycleEventListener, SurfaceHolder.Callback, View.OnAttachStateChangeListener {

    private static final String REACT_CLASS = "RCTGstPlayerController";

    // Native JNI c code bindings
    private native ByteBuffer jniCreateGstPlayer(String debugTag);
    private native void jniDestroyGstPlayer(ByteBuffer nativeGstPlayer);
    private native void jniSetParseLaunchPipeline(ByteBuffer nativeGstPlayer, String parseLaunchPipeline);
    private native void jniSetDrawableSurface(ByteBuffer nativeGstPlayer, Surface surface);
    private native void jniSetPipelineState(ByteBuffer nativeGstPlayer, int gstState);
    private native void jniSetPipelineProperties(ByteBuffer nativeGstPlayer, String pipelineProperties);

    // View
    private GstPlayerView view = null;

    // Properties
    private int playerIndex;
    private ReactContext context;
    private ByteBuffer nativeGstPlayer; // Contains the native C pointer to Gst Player
    private boolean playerReady = false;
    private int currentGstState = -1;

    // Player properties
    private String parseLaunchPipeline = null;
    private int pipelineState = -1;
    private String pipelineProperties = null;

    // Debug Tags
    private String getTag() {
        return String.format(Locale.getDefault(), "%s (%d - Java code)", REACT_CLASS, this.playerIndex);
    }

    private String getNativeTag() {
        return String.format(Locale.getDefault(), "%s (%d - Native code)", REACT_CLASS, this.playerIndex);
    }

    // Callbacks
    @SuppressWarnings("unused") // Used only from JNI
    public void onGstPlayerLoaded() {
        Log.i(this.getTag(), "onGstPlayerReady");

        this.playerReady = true;

        if (this.parseLaunchPipeline != null)
            this.jniSetParseLaunchPipeline(this.nativeGstPlayer, this.parseLaunchPipeline);

        if (this.pipelineState > -1)
            this.jniSetPipelineState(this.nativeGstPlayer, this.pipelineState);
    }

    @SuppressWarnings("unused") // Used only from JNI
    public void onGstPipelineStateChanged(int newState, int oldState) {
        Log.i(getTag(), "Pipeline state changed from " + oldState + " to " + newState);

        this.currentGstState = newState;

        WritableMap event = Arguments.createMap();
        event.putInt("newState", newState);
        event.putInt("oldState", oldState);
        context.getJSModule(RCTEventEmitter.class).receiveEvent(this.view.getId(),
                "onGstPipelineStateChanged", event);

        if (newState > oldState && newState == GstState.READY)
        {
            if (this.pipelineProperties != null)
                this.jniSetPipelineProperties(this.nativeGstPlayer, this.pipelineProperties);
        }
    }

    @SuppressWarnings("unused") // Used only from JNI
    public void onGstPipelineEOS() {
        Log.i(getTag(), "Pipeline EOS");

        context.getJSModule(RCTEventEmitter.class).receiveEvent(this.view.getId(),
                "onGstPipelineEOS", null);
    }

    @SuppressWarnings("WeakerAccess")
    public void onGstPipelineError(String source, String message, String debugInfo) {
        Log.i(getTag(), String.format("Pipeline Error from '%s' : %s. \n\tDebug informations : %s",
                source, message, debugInfo));

        WritableMap event = Arguments.createMap();

        event.putString("source", source);
        event.putString("message", message);
        event.putString("debugInfo", debugInfo);

        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onGstPipelineError", event
        );
    }

    @SuppressWarnings("unused")
    public void onGstElementMessage(String element, String message) {
        Log.i(getTag(), String.format("Pipeline Element message from '%s' : %s",
                element, message));

        WritableMap event = Arguments.createMap();
        event.putString("element", element);
        event.putString("message", message);

        context.getJSModule(RCTEventEmitter.class).receiveEvent(
                view.getId(), "onGstElementMessage", event
        );
    }

    // Getters
    int getPlayerIndex() {
        return this.playerIndex;
    }

    // Methods
    GstPlayerViewController(int playerIndex, ReactContext context) {
        this.playerIndex = playerIndex;
        this.context = context;
        this.nativeGstPlayer = this.jniCreateGstPlayer(this.getNativeTag());
    }

    GstPlayerView createView() {
        this.view = new GstPlayerView(this.context, this);
        return this.view;
    }

    void setParseLaunchPipeline(String parseLaunchPipeline) {
        Log.i(this.getTag(), "Set parseLaunchPipeline : " + parseLaunchPipeline);

        if (parseLaunchPipeline != null) {
            this.parseLaunchPipeline = parseLaunchPipeline;

            if (this.playerReady)
                this.jniSetParseLaunchPipeline(this.nativeGstPlayer, this.parseLaunchPipeline);
        }
    }

    void setPipelineState(int pipelineState) {
        Log.i(this.getTag(), "Set pipeline state : " + pipelineState);

        this.pipelineState = pipelineState;
        if (this.playerReady)
            this.jniSetPipelineState(this.nativeGstPlayer, this.pipelineState);
    }

    void setPipelineProperties(String pipelineProperties) {

        Log.i(this.getTag(), "Set pipeline properties : " + pipelineProperties);

        this.pipelineProperties = pipelineProperties;
        if (this.playerReady && this.currentGstState >= GstState.READY)
            this.jniSetPipelineProperties(this.nativeGstPlayer, this.pipelineProperties);
    }

    @Override
    public void onHostResume() {
        if (this.playerReady && this.currentGstState == 3 && this.pipelineState != this.currentGstState)
            this.jniSetPipelineState(this.nativeGstPlayer, this.pipelineState);
    }

    @Override
    public void onHostPause() {
        if (this.playerReady && this.currentGstState == 4)
            this.jniSetPipelineState(this.nativeGstPlayer, 3);
    }

    @Override
    public void onHostDestroy() {
        Log.i(getTag(), "Host destroyed");
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(getTag(), "Surface created: " + holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(getTag(), String.format(
                "Surface changed (%d).\n\tWidth : %d - Height : %d", format, width, height)
        );

        if (this.playerReady)
            this.jniSetDrawableSurface(this.nativeGstPlayer, holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(getTag(), "Surface destroyed: " + holder.getSurface());
    }

    @Override
    public void onViewAttachedToWindow(View v) {
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        if (this.playerReady) {
            this.jniDestroyGstPlayer(this.nativeGstPlayer);
            nativeGstPlayer.clear();
            nativeGstPlayer = null;
        }
    }
}
