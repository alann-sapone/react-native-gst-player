package com.asap.reactnativegstplayer;

import android.content.Context;
import android.view.SurfaceView;

import java.util.Locale;

public class GstPlayerView extends SurfaceView {

    public static final String REACT_CLASS = "RCTGstPlayerView";

    private GstPlayerViewController controller;

    public GstPlayerView(Context context, GstPlayerViewController controller) {
        super(context);

        this.controller = controller;

        this.getHolder().addCallback(this.controller);
        this.addOnAttachStateChangeListener(this.controller);
    }

    public String getTag() {
        return String.format(Locale.getDefault(), "%s (%d - Java code)", REACT_CLASS, this.getController().getPlayerIndex());
    }

    public GstPlayerViewController getController()
    {
        return this.controller;
    }
}
