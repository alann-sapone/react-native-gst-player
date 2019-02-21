//
//  GstPlayerManager
//  reactnativegstplayer
//
//  Created by Alann Sapone on 19/03/2019.
//  Copyright © 2019 Alann Sapone. All rights reserved.
//

#import "GstPlayerManager.h"
#import "GstPlayerView.h"

@implementation GstPlayerManager

static int RCT_GST_PLAYER_INDEX = 0;

- (instancetype)init
{
    self = [super init];
    if (self)
        gst_ios_init();

    return self;
}

+ (BOOL) requiresMainQueueSetup
{
    return TRUE;
}

RCT_EXPORT_MODULE(RCTGstPlayer);

RCT_EXPORT_VIEW_PROPERTY(onGstPipelineStateChanged, RCTDirectEventBlock)
RCT_EXPORT_VIEW_PROPERTY(onGstPipelineEOS, RCTDirectEventBlock)
RCT_EXPORT_VIEW_PROPERTY(onGstPipelineError, RCTDirectEventBlock)
RCT_EXPORT_VIEW_PROPERTY(onGstElementMessage, RCTDirectEventBlock)

RCT_CUSTOM_VIEW_PROPERTY(parseLaunchPipeline, NSString, GstPlayerView)
{
    NSString* parseLaunchPipeline = [RCTConvert NSString:json];
    [view setParseLaunchPipeline:parseLaunchPipeline];
}

RCT_CUSTOM_VIEW_PROPERTY(pipelineState, pipelineState, GstPlayerView)
{
    NSNumber* pipelineState = [RCTConvert NSNumber:json];
    [view setPipelineState:pipelineState];
}

RCT_CUSTOM_VIEW_PROPERTY(properties, NSString, GstPlayerView)
{
    NSString* properties = [RCTConvert NSString:json];
    [view setProperties:properties];
}

- (UIView *)view
{
    return [[GstPlayerView alloc] initWithPlayerIndex:++RCT_GST_PLAYER_INDEX];
}

@end
