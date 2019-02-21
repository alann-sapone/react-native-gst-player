//
//  GstPlayerView.h
//  reactnativegstplayer
//
//  Created by Alann Sapone on 19/03/2019.
//  Copyright © 2019 Alann Sapone. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <React/RCTViewManager.h>

#import "gst_ios_init.h"
#import "gst_player.h"

NS_ASSUME_NONNULL_BEGIN

@interface GstPlayerView : UIView {
    int playerIndex;
    gboolean playerReady;
    int currentGstState;
    RctGstPlayer *rct_gst_player;
}

@property (nonatomic, strong) NSString *parseLaunchPipeline;
@property (nonatomic, strong) NSNumber *pipelineState;
@property (nonatomic, strong) NSString *properties;

// react-native events
@property (nonatomic, copy) RCTDirectEventBlock onGstPipelineStateChanged;
@property (nonatomic, copy) RCTDirectEventBlock onGstPipelineEOS;
@property (nonatomic, copy) RCTDirectEventBlock onGstPipelineError;
@property (nonatomic, copy) RCTDirectEventBlock onGstElementMessage;


- (instancetype)initWithPlayerIndex:(int)playerIndex;

@end

NS_ASSUME_NONNULL_END
