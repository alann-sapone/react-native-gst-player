//
//  GstPlayerView.m
//  reactnativegstplayer
//
//  Created by Alann Sapone on 19/03/2019.
//  Copyright © 2019 Alann Sapone. All rights reserved.
//

#import "GstPlayerView.h"

@implementation GstPlayerView

static const gchar *REACT_CLASS = "RCTGstPlayerController";

// Open GL Drawable Surface
+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

- (NSString *)getTag
{
    return [NSString stringWithFormat:@"%s (%d - IOS code)", REACT_CLASS, self->playerIndex];
}

- (NSString *)getNativeTag
{
    return [NSString stringWithFormat:@"%s (%d - Native code)", REACT_CLASS, self->playerIndex];
}

static void cb_on_gst_player_loaded(RctGstPlayer *gst_player) {
    GstPlayerView *self = (__bridge GstPlayerView *)(rct_gst_player_get_user_data(gst_player));
    NSLog(@"%@ - Player is ready.", [self getTag]);
    
    self->playerReady = true;
    
    if (self->_parseLaunchPipeline != NULL)
        g_object_set(gst_player, "parse_launch_pipeline", [self->_parseLaunchPipeline UTF8String], NULL);
    
    if ([self->_pipelineState intValue] > -1)
        g_object_set(gst_player, "desired_state", [self->_pipelineState intValue], NULL);
    
}

static void cb_on_gst_pipeline_state_changed(RctGstPlayer *gst_player, GstState new_state, GstState old_state) {
    GstPlayerView *self = (__bridge GstPlayerView *)(rct_gst_player_get_user_data(gst_player));
    NSLog(@"%@ - Pipeline state changed from %s to %s.", [self getTag], gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
    
    self->currentGstState = new_state;

    self.onGstPipelineStateChanged(@{ @"oldState": [NSNumber numberWithInt:old_state], @"newState": [NSNumber numberWithInt:new_state] });
    
    if (new_state > old_state && new_state == GST_STATE_READY)
    {
        if (self->_properties != NULL)
        rct_gst_player_set_pipeline_properties(gst_player, [self->_properties UTF8String]);
    }
}

static void cb_on_gst_pipeline_eos(RctGstPlayer *gst_player) {
    GstPlayerView *self = (__bridge GstPlayerView *)(rct_gst_player_get_user_data(gst_player));
    NSLog(@"%@ - Pipeline EOS.", [self getTag]);
    
    self.onGstPipelineEOS(@{});
}

static void cb_on_gst_pipeline_error(RctGstPlayer *gst_player,
                                     const gchar *source,
                                     const gchar *message,
                                     const gchar *debug_info) {
    GstPlayerView *self = (__bridge GstPlayerView *)(rct_gst_player_get_user_data(gst_player));
    NSLog(@"%@ - Pipeline Error from '%s' : %s \n\tDebug informations : %s.", [self getTag], source, message, debug_info);
    
    self.onGstPipelineError(@{
                          @"source": [NSString stringWithUTF8String:source],
                          @"message": [NSString stringWithUTF8String:message],
                          @"debug_info": [NSString stringWithUTF8String:debug_info]
                          });
}

static void cb_on_gst_element_message(RctGstPlayer *gst_player,
                                      const gchar *element_name,
                                      const gchar *message) {
    GstPlayerView *self = (__bridge GstPlayerView *)(rct_gst_player_get_user_data(gst_player));
    NSLog(@"%@ - Pipeline Element message from '%s' : %s", [self getTag], element_name, message);
    
    self.onGstElementMessage(@{
                              @"element": [NSString stringWithUTF8String:element_name],
                              @"message": [NSString stringWithUTF8String:message],
                              });
}

- (instancetype)initWithPlayerIndex:(int)playerIndex
{
    self = [super init];
    if (self) {
        self->playerIndex = playerIndex;
        self->playerReady = false;
        self->currentGstState = -1;
    }
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onInactiveState) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onBackgroundState) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onActiveState) name:UIApplicationDidBecomeActiveNotification object:nil];
    
    return self;
}

-(void)onActiveState
{
    NSLog(@"%@ - Active state detected", [self getTag]);
    if (self->playerReady && self->currentGstState == 3 && [self->_pipelineState intValue] != self->currentGstState)
        g_object_set(self->rct_gst_player, "desired_state", [self->_pipelineState intValue], NULL);
    
}

-(void)onBackgroundState
{
    NSLog(@"%@ - Background state detected (Do nothing)", [self getTag]);
    // rct_gst_set_playbin_state([self getUserData], GST_STATE_READY);
}

-(void)onInactiveState
{
    NSLog(@"%@ - Inactive state detected", [self getTag]);
    if (self->playerReady && self->currentGstState == 4)
        g_object_set(self->rct_gst_player, "desired_state", 3, NULL);
}

- (void)setParseLaunchPipeline:(NSString *)parseLaunchPipeline
{
    NSLog(@"%@ - Set parseLaunchPipeline : %@", [self getTag], parseLaunchPipeline);
    
    if (parseLaunchPipeline != NULL) {
        _parseLaunchPipeline = parseLaunchPipeline;
        
        if (self->playerReady) {
            const gchar *str_parseLaunchPipeline = [_parseLaunchPipeline UTF8String];
            g_object_set(self->rct_gst_player, "parse_launch_pipeline", str_parseLaunchPipeline, NULL);
        }
    }
}

- (void)setPipelineState:(NSNumber *)pipelineState
{
    NSLog(@"%@ - Set pipeline state : %@", [self getTag], pipelineState);
    
    _pipelineState = pipelineState;
    if (self->playerReady) {
        g_object_set(self->rct_gst_player, "desired_state", [_pipelineState intValue], NULL);
    }
}

- (void)setProperties:(NSString *)properties
{
    NSLog(@"%@ - Set pipeline properties : %@", [self getTag], properties);
    
    _properties = properties;
    if (self->playerReady && self->currentGstState >= GST_STATE_READY) {
        const gchar *str_properties = [_properties UTF8String];
        rct_gst_player_set_pipeline_properties(self->rct_gst_player, str_properties);
    }
}

/**
 View lifecycle
 */

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    const gchar *debug_tag = [[self getNativeTag] UTF8String];
    self->rct_gst_player = rct_gst_player_new(debug_tag,
                                              cb_on_gst_player_loaded,
                                              cb_on_gst_pipeline_state_changed,
                                              cb_on_gst_pipeline_eos,
                                              cb_on_gst_pipeline_error,
                                              cb_on_gst_element_message,
                                              (__bridge gpointer)self);
    
    rct_gst_player_start(self->rct_gst_player);

    [super willMoveToWindow:newWindow];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    NSLog(@"%@ - Surface changed %p", [self getTag], self);
    
    if (self->playerReady) {
        g_object_set(self->rct_gst_player, "drawable_surface", self, NULL);
    }
}

- (void)removeFromSuperview
{
    if (self->playerReady) {
        NSLog(@"%@ - Destroying player %p", [self getTag], self);

        g_object_set(self->rct_gst_player, "desired_state", GST_STATE_NULL, NULL);
        rct_gst_player_stop(rct_gst_player);
        
        g_object_unref(rct_gst_player);
        self->rct_gst_player = NULL;

        [[NSNotificationCenter defaultCenter] removeObserver:self name:UIApplicationWillResignActiveNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:UIApplicationDidEnterBackgroundNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:UIApplicationDidBecomeActiveNotification object:nil];
         
    }
    
    [super removeFromSuperview];
}
@end
