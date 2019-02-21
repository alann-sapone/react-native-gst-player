import React from 'react';
import { requireNativeComponent, StyleSheet, View, Animated } from 'react-native';
import PropTypes from 'prop-types';
import * as utils from './utils';

class RCTGstPlayer extends React.Component {

    constructor(props, state, context) {
        super(props, state, context);
        this.state = {
            overlayOpacity: new Animated.Value(this.props.fadeOpacity),
            lastPropertiesDiff: this.props.properties
        }
    }

    componentDidUpdate(prevProps, prevState) {

        if (prevProps.parseLaunchPipeline !== this.props.parseLaunchPipeline) {
            this.setState({ lastPropertiesDiff: this.props.properties })
            return;
        }

        const currentProperties = this.props.properties;
        const oldProperties = prevProps.properties;

        let diffProperties = utils.substract(currentProperties, oldProperties);
        if (Object.keys(diffProperties).length > 0) {
            this.setState({ lastPropertiesDiff: diffProperties })
        }
    }

    onGstPipelineStateChanged(event) {
        const { newState, oldState } = event.nativeEvent;

        if (!this.props.onGstPipelineStateChanged)
            return;

        this.props.onGstPipelineStateChanged(newState, oldState);

        // Lighten | Darken Player
        const newOverlayOpacity = newState >= 3 ? 0 : this.props.fadeOpacity;
        Animated.timing(
            this.state.overlayOpacity, {
                toValue: newOverlayOpacity,
                duration: this.props.fadeSpeed,
            }
        ).start();
    }

    onGstPipelineEOS() {
        if (this.props.onGstPipelineEOS)
            this.props.onGstPipelineEOS()
    }

    onGstPipelineError(event) {
        const { source, message, debugInfo } = event.nativeEvent;

        if (this.props.onGstPipelineError)
            this.props.onGstPipelineError(source, message, debugInfo)
    }

    onGstElementMessage(event) {
        const { element, message } = event.nativeEvent;
        try {
            const jsonMessage = JSON.parse(message);
            if (this.props.onGstElementMessage)
                this.props.onGstElementMessage(element, jsonMessage)
        } catch (error) {
            if (this.props.onGstPipelineError)
                this.props.onGstPipelineError(element, "Serialization error", null)
        }

        
    }
    render() {
        let { overlayOpacity } = this.state;

        return (
            <View style={[styles.container, this.props.style]}>
                <RCTGstPlayerNative
                    parseLaunchPipeline={this.props.parseLaunchPipeline}
                    pipelineState={this.props.pipelineState}
                    properties={JSON.stringify(this.state.lastPropertiesDiff)}

                    onGstPipelineStateChanged={this.onGstPipelineStateChanged.bind(this)}
                    onGstPipelineEOS={this.onGstPipelineEOS.bind(this)}
                    onGstPipelineError={this.onGstPipelineError.bind(this)}
                    onGstElementMessage={this.onGstElementMessage.bind(this)}

                    style={styles.player}

                />
                <Animated.View style={[styles.overlay, {
                    opacity: overlayOpacity
                }]} />
            </View>
        )
    }
}

const styles = StyleSheet.create({
    container: {
        flex: 1,
    },
    player: {
        flex: 1,
    },
    overlay: {
        position: 'absolute',
        top: 0,
        right: 0,
        bottom: 0,
        left: 0,
        backgroundColor: '#000'
    }
});

let RCTGstPlayerNative = requireNativeComponent('RCTGstPlayer', null)

RCTGstPlayer.propTypes = {
    // Overlay
    fadeSpeed: PropTypes.number.isRequired,
    fadeOpacity: PropTypes.number.isRequired,

    // Pipeline
    parseLaunchPipeline: PropTypes.string.isRequired,
    pipelineState: PropTypes.number.isRequired,
    properties: PropTypes.object,

    // Pipeline callbacks
    onGstPipelineStateChanged: PropTypes.func,
    onGstPipelineEOS: PropTypes.func,
    onGstPipelineError: PropTypes.func,
    onGstElementMessage: PropTypes.func
};

RCTGstPlayer.defaultProps = {
    // Overlay
    fadeSpeed: 225,
    fadeOpacity: 1,

    // Pipeline
    pipelineState: 2,
    properties: {}
};

export default RCTGstPlayer;