// C wrapper implementation for AASDK
// This provides a C interface on top of AASDK's C++ API

#include "aasdk_c.h"

#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

// AASDK includes
#include <f1x/aasdk/IO/IOContextWrapper.hpp>
#include <f1x/aasdk/USB/USBWrapper.hpp>
#include <f1x/aasdk/USB/USBHub.hpp>
#include <f1x/aasdk/USB/ConnectedAccessoriesEnumerator.hpp>
#include <f1x/aasdk/USB/AccessoryModeQueryChainFactory.hpp>
#include <f1x/aasdk/USB/AccessoryModeQueryFactory.hpp>
#include <f1x/aasdk/IO/Promise.hpp>
#include <f1x/aasdk/Transport/USBTransport.hpp>
#include <f1x/aasdk/Transport/SSLWrapper.hpp>
#include <f1x/aasdk/Messenger/Messenger.hpp>
#include <f1x/aasdk/Messenger/MessageInStream.hpp>
#include <f1x/aasdk/Messenger/MessageOutStream.hpp>
#include <f1x/aasdk/Messenger/Cryptor.hpp>
#include <f1x/aasdk/Channel/AV/VideoServiceChannel.hpp>
#include <f1x/aasdk/Channel/AV/AudioServiceChannel.hpp>
#include <f1x/aasdk/Channel/Input/InputServiceChannel.hpp>
#include <f1x/aasdk/Channel/Control/ControlServiceChannel.hpp>
#include <f1x/aasdk/Channel/AV/MediaAudioServiceChannel.hpp>
#include <f1x/aasdk/Channel/AV/SpeechAudioServiceChannel.hpp>
#include <f1x/aasdk/Channel/AV/SystemAudioServiceChannel.hpp>
#include <f1x/aasdk/USB/AOAPDevice.hpp>
#include <libusb-1.0/libusb.h>
#include <boost/asio.hpp>
#include <iostream>

using namespace f1x::aasdk;

// Forward declarations
struct AASDKContext;

// Event handlers that forward to C callbacks
class VideoEventHandler : public channel::av::IVideoServiceChannelEventHandler {
public:
    VideoEventHandler(VideoFrameCallback cb, void* ud, AASDKContext* ctx)
        : callback_(cb), user_data_(ud), ctx_(ctx),
          video_width_(1280), video_height_(720) {}

    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) override;
    void onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) override;

    void onAVChannelStartIndication(const proto::messages::AVChannelStartIndication& indication) override;
    void onAVChannelStopIndication(const proto::messages::AVChannelStopIndication& indication) override;

    void onAVMediaWithTimestampIndication(messenger::Timestamp::ValueType timestamp, const common::DataConstBuffer& buffer) override {
        // Don't log every frame - too verbose
        if (callback_ && buffer.cdata && buffer.size > 0) {
            uint32_t buffer_size = static_cast<uint32_t>(buffer.size);
            callback_(buffer.cdata, video_width_, video_height_, buffer_size, user_data_);
        }
    }

    void onAVMediaIndication(const common::DataConstBuffer& buffer) override {
        // Don't log every frame - too verbose
        if (callback_ && buffer.cdata && buffer.size > 0) {
            uint32_t buffer_size = static_cast<uint32_t>(buffer.size);
            callback_(buffer.cdata, video_width_, video_height_, buffer_size, user_data_);
        }
    }

    void onVideoFocusRequest(const proto::messages::VideoFocusRequest& request) override;
    void onChannelError(const error::Error& e) override;

private:
    VideoFrameCallback callback_;
    void* user_data_;
    AASDKContext* ctx_;
    uint32_t video_width_;
    uint32_t video_height_;
};

class AudioEventHandler : public channel::av::IAudioServiceChannelEventHandler {
public:
    AudioEventHandler(AudioDataCallback cb, void* ud, AASDKContext* ctx, channel::av::AudioServiceChannel::Pointer* channel_ptr)
        : callback_(cb), user_data_(ud), ctx_(ctx), channel_ptr_(channel_ptr),
          sample_rate_(48000), channels_(2), bit_depth_(16) {}

    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) override;
    void onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) override;

    void onAVChannelStartIndication(const proto::messages::AVChannelStartIndication& indication) override {
        std::cerr << "Audio stream started" << std::endl;

        // Continue receiving on audio channel
        if (ctx_ && channel_ptr_ && *channel_ptr_) {
            // Audio event handlers are shared pointers managed by context
        }
    }

    void onAVChannelStopIndication(const proto::messages::AVChannelStopIndication& indication) override {
        std::cerr << "Audio stream stopped" << std::endl;

        // Continue receiving on audio channel
        if (ctx_ && channel_ptr_ && *channel_ptr_) {
            // Audio event handlers are shared pointers managed by context
        }
    }

    void onAVMediaWithTimestampIndication(messenger::Timestamp::ValueType timestamp, const common::DataConstBuffer& buffer) override {
        if (callback_ && buffer.cdata) {
            // Use configured audio parameters
            const int16_t* samples = reinterpret_cast<const int16_t*>(buffer.cdata);
            uint32_t sample_count = buffer.size / (bit_depth_ / 8);
            callback_(samples, sample_count, channels_, sample_rate_, user_data_);
        }
    }

    void onAVMediaIndication(const common::DataConstBuffer& buffer) override {
        if (callback_ && buffer.cdata) {
            // Use configured audio parameters
            const int16_t* samples = reinterpret_cast<const int16_t*>(buffer.cdata);
            uint32_t sample_count = buffer.size / (bit_depth_ / 8);
            callback_(samples, sample_count, channels_, sample_rate_, user_data_);
        }
    }

    void onChannelError(const error::Error& e) override {
        std::cerr << "Audio channel error: " << e.what()
                  << " (code: " << (int)e.getCode() << ", native: " << e.getNativeCode() << ")" << std::endl;
    }

private:
    AudioDataCallback callback_;
    void* user_data_;
    AASDKContext* ctx_;
    channel::av::AudioServiceChannel::Pointer* channel_ptr_;
    uint32_t sample_rate_;
    uint32_t channels_;
    uint32_t bit_depth_;
};

// Control channel event handler (uses forward declaration, methods implemented after AASDKContext is defined)
class ControlEventHandler : public channel::control::IControlServiceChannelEventHandler {
public:
    ControlEventHandler(AASDKContext* ctx) : ctx_(ctx) {}
    
    void onVersionResponse(uint16_t majorCode, uint16_t minorCode, proto::enums::VersionResponseStatus::Enum status) override;
    void onHandshake(const common::DataConstBuffer& payload) override;
    void onServiceDiscoveryRequest(const proto::messages::ServiceDiscoveryRequest& request) override;
    void onAudioFocusRequest(const proto::messages::AudioFocusRequest& request) override;
    void onShutdownRequest(const proto::messages::ShutdownRequest& request) override;
    void onShutdownResponse(const proto::messages::ShutdownResponse& response) override;
    void onNavigationFocusRequest(const proto::messages::NavigationFocusRequest& request) override;
    void onPingResponse(const proto::messages::PingResponse& response) override;
    void onChannelError(const error::Error& e) override;

private:
    AASDKContext* ctx_;
};

// Main AASDK context
struct AASDKContext {
    boost::asio::io_service ioService;
    std::unique_ptr<boost::asio::io_service::work> work;
    std::thread ioThread;
    
    libusb_context* usbContext;
    std::unique_ptr<usb::USBWrapper> usbWrapper;
    std::unique_ptr<usb::AccessoryModeQueryFactory> queryFactory;
    std::unique_ptr<usb::AccessoryModeQueryChainFactory> queryChainFactory;
    usb::IUSBHub::Pointer usbHub;  // Must be shared_ptr because USBHub uses shared_from_this()
    usb::IAccessoryModeQueryChain::Pointer activeQueryChain;  // For enumerating already-connected devices
    
    usb::IAOAPDevice::Pointer aoapDevice;
    transport::USBTransport::Pointer transport;
    messenger::ICryptor::Pointer cryptor;
    messenger::Messenger::Pointer messenger;
    messenger::MessageInStream::Pointer messageInStream;
    messenger::MessageOutStream::Pointer messageOutStream;
    
    channel::av::VideoServiceChannel::Pointer videoChannel;
    channel::av::AudioServiceChannel::Pointer mediaAudioChannel;
    channel::av::AudioServiceChannel::Pointer speechAudioChannel;
    channel::av::AudioServiceChannel::Pointer systemAudioChannel;
    channel::input::InputServiceChannel::Pointer inputChannel;
    channel::control::ControlServiceChannel::Pointer controlChannel;

    // Strands for channel thread safety - must be kept alive
    std::unique_ptr<boost::asio::io_service::strand> controlStrand;
    std::unique_ptr<boost::asio::io_service::strand> videoStrand;
    std::unique_ptr<boost::asio::io_service::strand> mediaAudioStrand;
    std::unique_ptr<boost::asio::io_service::strand> speechAudioStrand;
    std::unique_ptr<boost::asio::io_service::strand> systemAudioStrand;
    std::unique_ptr<boost::asio::io_service::strand> inputStrand;

    std::shared_ptr<VideoEventHandler> videoEventHandler;
    std::shared_ptr<AudioEventHandler> audioEventHandler;
    std::shared_ptr<AudioEventHandler> speechAudioEventHandler;
    std::shared_ptr<AudioEventHandler> systemAudioEventHandler;
    std::shared_ptr<ControlEventHandler> controlEventHandler;
    
    VideoFrameCallback videoCallback;
    AudioDataCallback audioCallback;
    ConnectionStatusCallback connectionCallback;
    void* userData;
    
    std::atomic<bool> connected;
    std::atomic<bool> running;
    std::mutex mutex;
    
    AASDKContext() : connected(false), running(false) {}
    
    ~AASDKContext() {
        stop();
    }
    
    void stop() {
        if (running) {
            running = false;
            work.reset();
            if (ioThread.joinable()) {
                ioThread.join();
            }
            if (usbHub) {
                usbHub->cancel();
            }
            if (activeQueryChain) {
                activeQueryChain->cancel();
            }
            if (transport) {
                transport->stop();
            }
            if (messenger) {
                messenger->stop();
            }
        }
    }
};

// Implement VideoEventHandler methods (after AASDKContext is defined)
void VideoEventHandler::onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) {
    std::cerr << "Video channel open request, priority: " << request.priority() << std::endl;

    if (!ctx_ || !ctx_->videoChannel) {
        std::cerr << "Error: videoChannel not available" << std::endl;
        return;
    }

    // Send channel open response
    proto::messages::ChannelOpenResponse response;
    response.set_status(proto::enums::Status::OK);

    auto promise = channel::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Video channel open response sent" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send video channel open response: " << e.what() << std::endl;
    });

    ctx_->videoChannel->sendChannelOpenResponse(response, std::move(promise));

    // Continue receiving on video channel
    if (ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

void VideoEventHandler::onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) {
    std::cerr << "Video setup request received, config_index: " << request.config_index() << std::endl;

    if (!ctx_ || !ctx_->videoChannel) {
        std::cerr << "Error: videoChannel not available" << std::endl;
        return;
    }

    // TODO: Parse the actual video configuration based on config_index
    // For now, assume standard 1280x720 H264 video
    video_width_ = 1280;
    video_height_ = 720;

    // Send setup response accepting the configuration
    proto::messages::AVChannelSetupResponse response;
    response.set_media_status(proto::enums::AVChannelSetupStatus::OK);
    response.set_max_unacked(1);  // Allow 1 unacknowledged frame
    response.add_configs(request.config_index());  // Accept the requested config

    std::cerr << "Accepting video config " << request.config_index() << std::endl;

    auto promise = channel::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Video setup response sent" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send video setup response: " << e.what() << std::endl;
    });

    ctx_->videoChannel->sendAVChannelSetupResponse(response, std::move(promise));

    // Continue receiving on video channel
    if (ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

void VideoEventHandler::onAVChannelStartIndication(const proto::messages::AVChannelStartIndication& /*indication*/) {
    std::cerr << "Video stream started" << std::endl;
    // Video frames will now start arriving in onAVMediaIndication/onAVMediaWithTimestampIndication

    // Continue receiving on video channel
    if (ctx_ && ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

void VideoEventHandler::onAVChannelStopIndication(const proto::messages::AVChannelStopIndication& /*indication*/) {
    std::cerr << "Video stream stopped" << std::endl;

    // Continue receiving on video channel
    if (ctx_ && ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

void VideoEventHandler::onVideoFocusRequest(const proto::messages::VideoFocusRequest& request) {
    std::cerr << "Video focus request received, mode: " << request.focus_mode()
              << ", reason: " << request.focus_reason() << std::endl;

    if (!ctx_ || !ctx_->videoChannel) {
        std::cerr << "Error: videoChannel not available for focus request" << std::endl;
        return;
    }

    // Send video focus indication to grant focus
    proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(request.focus_mode());
    indication.set_unrequested(false);

    std::cerr << "Sending video focus indication (granting focus)" << std::endl;

    auto promise = channel::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Video focus indication sent successfully" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send video focus indication: " << e.what() << std::endl;
    });

    ctx_->videoChannel->sendVideoFocusIndication(indication, std::move(promise));

    // Continue receiving on video channel
    if (ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

void VideoEventHandler::onChannelError(const error::Error& e) {
    std::cerr << "Video channel error: " << e.what()
              << " (code: " << (int)e.getCode() << ", native: " << e.getNativeCode() << ")" << std::endl;

    // Try to continue receiving despite error
    if (ctx_ && ctx_->videoChannel && ctx_->videoEventHandler) {
        ctx_->videoChannel->receive(ctx_->videoEventHandler);
    }
}

// Implement AudioEventHandler methods (after AASDKContext is defined)
void AudioEventHandler::onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) {
    std::cerr << "Audio channel open request, priority: " << request.priority() << std::endl;

    if (!ctx_ || !channel_ptr_ || !*channel_ptr_) {
        std::cerr << "Error: audio channel not available" << std::endl;
        return;
    }

    // Send channel open response
    proto::messages::ChannelOpenResponse response;
    response.set_status(proto::enums::Status::OK);

    auto promise = channel::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Audio channel open response sent" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send audio channel open response: " << e.what() << std::endl;
    });

    (*channel_ptr_)->sendChannelOpenResponse(response, std::move(promise));

    // Continue receiving on audio channel - handler is managed by context
    // The channel will automatically continue receiving after each message
}

void AudioEventHandler::onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) {
    std::cerr << "Audio setup request received, config_index: " << request.config_index() << std::endl;

    if (!ctx_ || !channel_ptr_ || !*channel_ptr_) {
        std::cerr << "Error: audio channel not available" << std::endl;
        return;
    }

    // TODO: Parse the actual audio configuration based on config_index
    // For now, assume standard 48kHz 16-bit stereo PCM
    sample_rate_ = 48000;
    channels_ = 2;
    bit_depth_ = 16;

    // Send setup response accepting the configuration
    proto::messages::AVChannelSetupResponse response;
    response.set_media_status(proto::enums::AVChannelSetupStatus::OK);
    response.set_max_unacked(1);  // Allow 1 unacknowledged frame
    response.add_configs(request.config_index());  // Accept the requested config

    std::cerr << "Accepting audio config " << request.config_index() << std::endl;

    auto promise = channel::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Audio setup response sent" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send audio setup response: " << e.what() << std::endl;
    });

    (*channel_ptr_)->sendAVChannelSetupResponse(response, std::move(promise));

    // Continue receiving on audio channel - handler is managed by context
    // The channel will automatically continue receiving after each message
}

// Helper function to set up device connection
static void setupDeviceConnection(AASDKContext* ctx, usb::DeviceHandle deviceHandle) {
    try {
        std::cerr << "Setting up device connection..." << std::endl;

        // Detach kernel driver if active (fixes LIBUSB_ERROR_BUSY)
        // Check interface 0 (AOAP uses interface 0)
        int kernelDriverActive = libusb_kernel_driver_active(deviceHandle.get(), 0);
        if (kernelDriverActive == 1) {
            std::cerr << "Kernel driver active on interface 0, detaching..." << std::endl;
            int detachResult = libusb_detach_kernel_driver(deviceHandle.get(), 0);
            if (detachResult == 0) {
                std::cerr << "Successfully detached kernel driver" << std::endl;
            } else {
                std::cerr << "Warning: Failed to detach kernel driver: " << libusb_error_name(detachResult) << std::endl;
            }
        } else if (kernelDriverActive == 0) {
            std::cerr << "No kernel driver active on interface 0" << std::endl;
        } else {
            std::cerr << "Warning: Could not check kernel driver status: " << libusb_error_name(kernelDriverActive) << std::endl;
        }

        // Create AOAPDevice from handle
        ctx->aoapDevice = usb::AOAPDevice::create(*ctx->usbWrapper, ctx->ioService, deviceHandle);
        if (!ctx->aoapDevice) {
            std::cerr << "Failed to create AOAPDevice" << std::endl;
            return;
        }
        
        // Create USB transport
        ctx->transport = std::make_shared<transport::USBTransport>(ctx->ioService, ctx->aoapDevice);
        
        // Create SSL wrapper
        auto sslWrapper = std::make_shared<transport::SSLWrapper>();

        // Create cryptor and store it in context
        ctx->cryptor = std::make_shared<messenger::Cryptor>(sslWrapper);
        ctx->cryptor->init();

        // Create message streams using the stored cryptor
        ctx->messageInStream = std::make_shared<messenger::MessageInStream>(
            ctx->ioService, ctx->transport, ctx->cryptor
        );
        ctx->messageOutStream = std::make_shared<messenger::MessageOutStream>(
            ctx->ioService, ctx->transport, ctx->cryptor
        );
        
        // Create messenger
        ctx->messenger = std::make_shared<messenger::Messenger>(
            ctx->ioService, ctx->messageInStream, ctx->messageOutStream
        );

        // Create control strand and store it to keep it alive
        ctx->controlStrand = std::make_unique<boost::asio::io_service::strand>(ctx->ioService);

        // Create control channel using the stored strand
        ctx->controlChannel = std::make_shared<channel::control::ControlServiceChannel>(
            *ctx->controlStrand, ctx->messenger
        );
        
        // Create control event handler and store it in context to keep it alive
        ctx->controlEventHandler = std::make_shared<ControlEventHandler>(ctx);
        
        // Start receiving on control channel
        ctx->controlChannel->receive(ctx->controlEventHandler);
        
        // Send version request to start handshake
        auto versionPromise = messenger::SendPromise::defer(ctx->ioService);
        versionPromise->then([]() {
            std::cerr << "Version request sent" << std::endl;
        }, [](const error::Error& e) {
            std::cerr << "Version request failed: " << e.what() << std::endl;
        });
        ctx->controlChannel->sendVersionRequest(std::move(versionPromise));
        
        std::cerr << "Device connection setup complete, starting handshake..." << std::endl;
        
        // Report connection status (device discovered, handshake in progress)
        if (ctx->connectionCallback) {
            ctx->connectionCallback(true, ctx->userData);
        }
        ctx->connected = true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to set up device connection: " << e.what() << std::endl;
        if (ctx->connectionCallback) {
            ctx->connectionCallback(false, ctx->userData);
        }
    }
}

// Implement ControlEventHandler methods (after AASDKContext is defined)
void ControlEventHandler::onVersionResponse(uint16_t majorCode, uint16_t minorCode, proto::enums::VersionResponseStatus::Enum status) {
    std::cerr << "Version response: " << majorCode << "." << minorCode << " status: " << (int)status << std::endl;

    if (!ctx_ || !ctx_->controlChannel || !ctx_->cryptor) {
        std::cerr << "ERROR: Cannot initiate handshake - required components missing" << std::endl;
        return;
    }

    if (status == proto::enums::VersionResponseStatus::MISMATCH) {
        std::cerr << "ERROR: Version mismatch!" << std::endl;
        return;
    }

    std::cerr << "Begin SSL handshake..." << std::endl;

    try {
        // Initiate SSL handshake
        ctx_->cryptor->doHandshake();

        // Read the handshake data we generated
        auto handshakeBuffer = ctx_->cryptor->readHandshakeBuffer();

        std::cerr << "Sending initial SSL handshake to phone, size: " << handshakeBuffer.size() << std::endl;

        // Send our handshake to the phone
        auto promise = messenger::SendPromise::defer(ctx_->ioService);
        promise->then([]() {
            std::cerr << "Initial SSL handshake sent successfully" << std::endl;
        }, [](const error::Error& e) {
            std::cerr << "Failed to send initial SSL handshake: " << e.what() << std::endl;
        });

        ctx_->controlChannel->sendHandshake(std::move(handshakeBuffer), std::move(promise));

        // Now wait for the phone's response
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
        std::cerr << "Waiting for phone's SSL handshake response..." << std::endl;

    } catch (const error::Error& e) {
        std::cerr << "Handshake error: " << e.what() << std::endl;
    }
}

void ControlEventHandler::onHandshake(const common::DataConstBuffer& payload) {
    std::cerr << "Handshake received from phone, payload size: " << payload.size << std::endl;

    if (!ctx_ || !ctx_->controlChannel || !ctx_->cryptor) {
        std::cerr << "Error: Required components not available for handshake" << std::endl;
        return;
    }

    try {
        // Write the phone's handshake data to the cryptor
        ctx_->cryptor->writeHandshakeBuffer(payload);

        // Continue the SSL handshake
        if (!ctx_->cryptor->doHandshake()) {
            // Handshake not complete yet, need to send more data
            std::cerr << "Continue SSL handshake..." << std::endl;

            auto handshakeBuffer = ctx_->cryptor->readHandshakeBuffer();
            std::cerr << "Sending handshake continuation to phone, size: " << handshakeBuffer.size() << std::endl;

            auto promise = messenger::SendPromise::defer(ctx_->ioService);
            promise->then([]() {
                std::cerr << "Handshake continuation sent successfully" << std::endl;
            }, [](const error::Error& e) {
                std::cerr << "Failed to send handshake continuation: " << e.what() << std::endl;
            });

            ctx_->controlChannel->sendHandshake(std::move(handshakeBuffer), std::move(promise));
        } else {
            // SSL handshake is complete!
            std::cerr << "SSL handshake completed successfully! Sending Auth Complete..." << std::endl;

            proto::messages::AuthCompleteIndication authCompleteIndication;
            authCompleteIndication.set_status(proto::enums::Status::OK);

            auto authPromise = messenger::SendPromise::defer(ctx_->ioService);
            authPromise->then([]() {
                std::cerr << "Auth complete sent, waiting for service discovery request..." << std::endl;
            }, [](const error::Error& e) {
                std::cerr << "Failed to send auth complete: " << e.what() << std::endl;
            });

            ctx_->controlChannel->sendAuthComplete(authCompleteIndication, std::move(authPromise));
        }

        // Always re-register to receive the next message
        ctx_->controlChannel->receive(ctx_->controlEventHandler);

    } catch (const error::Error& e) {
        std::cerr << "Handshake error: " << e.what() << std::endl;
    }
}

void ControlEventHandler::onServiceDiscoveryRequest(const proto::messages::ServiceDiscoveryRequest& request) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::cerr << "[" << ms << "ms] Service discovery request received" << std::endl;

    if (!ctx_) {
        std::cerr << "Context is null in onServiceDiscoveryRequest" << std::endl;
        return;
    }

    // Create service discovery response
    proto::messages::ServiceDiscoveryResponse response;

    // Set head unit information (CRITICAL - OpenAuto sets these!)
    response.set_head_unit_name("GolfCartAuto");
    response.set_car_model("Golf Cart");
    response.set_car_year("2025");
    response.set_car_serial("GC001");
    response.set_left_hand_drive_vehicle(true);  // Left-hand drive
    response.set_headunit_manufacturer("Custom");
    response.set_headunit_model("Infotainment v1");
    response.set_sw_build("1.0.0");
    response.set_sw_version("1.0");
    response.set_can_play_native_media_during_vr(false);
    response.set_hide_clock(false);

    // Add channels in the EXACT order that OpenAuto uses (critical!)
    // Order: AV_INPUT, MEDIA_AUDIO, SPEECH_AUDIO, SYSTEM_AUDIO, SENSOR, VIDEO, BLUETOOTH, INPUT

    // 1. Add AV Input service (MANDATORY - for microphone/voice commands) - FIRST!
    auto* avInputService = response.add_channels();
    avInputService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::AV_INPUT));
    auto* avInputChannelData = avInputService->mutable_av_input_channel();
    avInputChannelData->set_stream_type(proto::enums::AVStreamType::AUDIO);
    avInputChannelData->set_available_while_in_call(true);
    auto* avInputConfig = avInputChannelData->mutable_audio_config();
    avInputConfig->set_sample_rate(16000);
    avInputConfig->set_bit_depth(16);
    avInputConfig->set_channel_count(1);

    // 2. Add media audio service with configuration
    auto* mediaAudioService = response.add_channels();
    mediaAudioService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::MEDIA_AUDIO));
    auto* mediaAudioChannelData = mediaAudioService->mutable_av_channel();
    mediaAudioChannelData->set_stream_type(proto::enums::AVStreamType::AUDIO);
    mediaAudioChannelData->set_audio_type(proto::enums::AudioType::MEDIA);
    mediaAudioChannelData->set_available_while_in_call(false);
    auto* mediaAudioConfig = mediaAudioChannelData->add_audio_configs();
    mediaAudioConfig->set_sample_rate(48000);
    mediaAudioConfig->set_bit_depth(16);
    mediaAudioConfig->set_channel_count(2);

    // 3. Add speech audio service with configuration
    auto* speechAudioService = response.add_channels();
    speechAudioService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::SPEECH_AUDIO));
    auto* speechAudioChannelData = speechAudioService->mutable_av_channel();
    speechAudioChannelData->set_stream_type(proto::enums::AVStreamType::AUDIO);
    speechAudioChannelData->set_audio_type(proto::enums::AudioType::SPEECH);
    speechAudioChannelData->set_available_while_in_call(true);
    auto* speechAudioConfig = speechAudioChannelData->add_audio_configs();
    speechAudioConfig->set_sample_rate(16000);
    speechAudioConfig->set_bit_depth(16);
    speechAudioConfig->set_channel_count(1);

    // 4. Add system audio service with configuration
    auto* systemAudioService = response.add_channels();
    systemAudioService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::SYSTEM_AUDIO));
    auto* systemAudioChannelData = systemAudioService->mutable_av_channel();
    systemAudioChannelData->set_stream_type(proto::enums::AVStreamType::AUDIO);
    systemAudioChannelData->set_audio_type(proto::enums::AudioType::SYSTEM);
    systemAudioChannelData->set_available_while_in_call(true);
    auto* systemAudioConfig = systemAudioChannelData->add_audio_configs();
    systemAudioConfig->set_sample_rate(16000);
    systemAudioConfig->set_bit_depth(16);
    systemAudioConfig->set_channel_count(1);

    // 5. Add sensor service (GPS, etc.)
    auto* sensorService = response.add_channels();
    sensorService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::SENSOR));
    // Sensor channel data is optional, phone will query for specific sensors

    // 6. Add video service with configuration
    auto* videoService = response.add_channels();
    videoService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::VIDEO));
    auto* videoChannelData = videoService->mutable_av_channel();
    videoChannelData->set_stream_type(proto::enums::AVStreamType::VIDEO);
    videoChannelData->set_available_while_in_call(true);  // Match OpenAuto

    // Add supported video configurations (provide multiple options for phone to choose)
    // Primary: 480p at 60fps (matches OpenAuto defaults)
    auto* videoConfig480p60 = videoChannelData->add_video_configs();
    videoConfig480p60->set_video_resolution(proto::enums::VideoResolution::_480p);
    videoConfig480p60->set_video_fps(proto::enums::VideoFPS::_60);
    videoConfig480p60->set_margin_width(0);
    videoConfig480p60->set_margin_height(0);
    videoConfig480p60->set_dpi(140);  // Match OpenAuto
    videoConfig480p60->set_additional_depth(0);

    // Alternative: 720p at 60fps
    auto* videoConfig720p60 = videoChannelData->add_video_configs();
    videoConfig720p60->set_video_resolution(proto::enums::VideoResolution::_720p);
    videoConfig720p60->set_video_fps(proto::enums::VideoFPS::_60);
    videoConfig720p60->set_margin_width(0);
    videoConfig720p60->set_margin_height(0);
    videoConfig720p60->set_dpi(140);
    videoConfig720p60->set_additional_depth(0);

    // Store primary config for logging
    auto* videoConfig = videoConfig480p60;

    // 7. Add Bluetooth service (MANDATORY - for phone pairing)
    auto* bluetoothService = response.add_channels();
    bluetoothService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::BLUETOOTH));
    auto* bluetoothChannelData = bluetoothService->mutable_bluetooth_channel();
    // Set a dummy Bluetooth MAC address (format: XX:XX:XX:XX:XX:XX)
    bluetoothChannelData->set_adapter_address("00:00:00:00:00:00");

    // 8. Add input service (touchscreen, buttons) with configuration - LAST
    auto* inputService = response.add_channels();
    inputService->set_channel_id(static_cast<uint32_t>(messenger::ChannelId::INPUT));
    auto* inputChannelData = inputService->mutable_input_channel();
    // Add supported button keycodes (common Android Auto buttons)
    inputChannelData->add_supported_keycodes(1); // KEYCODE_BACK
    inputChannelData->add_supported_keycodes(3); // KEYCODE_HOME
    inputChannelData->add_supported_keycodes(24); // KEYCODE_VOLUME_UP
    inputChannelData->add_supported_keycodes(25); // KEYCODE_VOLUME_DOWN
    inputChannelData->add_supported_keycodes(85); // KEYCODE_MEDIA_PLAY_PAUSE
    inputChannelData->add_supported_keycodes(87); // KEYCODE_MEDIA_NEXT
    inputChannelData->add_supported_keycodes(88); // KEYCODE_MEDIA_PREVIOUS
    inputChannelData->add_supported_keycodes(126); // KEYCODE_MEDIA_PLAY
    inputChannelData->add_supported_keycodes(127); // KEYCODE_MEDIA_PAUSE
    // Add touchscreen configuration: 1280x720 display
    auto* touchConfig = inputChannelData->mutable_touch_screen_config();
    touchConfig->set_width(1280);
    touchConfig->set_height(720);

    std::cerr << "Sending service discovery response with " << response.channels_size() << " services (with full config data)" << std::endl;
    std::cerr << "Video config: resolution=" << videoConfig->video_resolution()
              << " fps=" << videoConfig->video_fps()
              << " " << videoConfig->margin_width() << "x" << videoConfig->margin_height() << std::endl;
    std::cerr << "Media audio config: " << mediaAudioConfig->sample_rate() << "Hz "
              << mediaAudioConfig->bit_depth() << "bit "
              << mediaAudioConfig->channel_count() << "ch" << std::endl;
    std::cerr << "Touch config: " << touchConfig->width() << "x" << touchConfig->height() << std::endl;

    // Send the response
    auto promise = messenger::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Service discovery response sent successfully" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send service discovery response: " << e.what() << std::endl;
    });

    ctx_->controlChannel->sendServiceDiscoveryResponse(response, std::move(promise));

    // Now set up the service channels
    std::cerr << "Setting up service channels..." << std::endl;

    // Create video strand and channel
    ctx_->videoStrand = std::make_unique<boost::asio::io_service::strand>(ctx_->ioService);
    ctx_->videoChannel = std::make_shared<channel::av::VideoServiceChannel>(
        *ctx_->videoStrand, ctx_->messenger
    );

    // Create video event handler
    ctx_->videoEventHandler = std::make_shared<VideoEventHandler>(ctx_->videoCallback, ctx_->userData, ctx_);
    ctx_->videoChannel->receive(ctx_->videoEventHandler);

    std::cerr << "Video channel setup complete" << std::endl;

    // Create media audio strand and channel
    ctx_->mediaAudioStrand = std::make_unique<boost::asio::io_service::strand>(ctx_->ioService);
    ctx_->mediaAudioChannel = std::make_shared<channel::av::MediaAudioServiceChannel>(
        *ctx_->mediaAudioStrand, ctx_->messenger
    );

    // Create audio event handler for media, passing pointer to the channel
    ctx_->audioEventHandler = std::make_shared<AudioEventHandler>(
        ctx_->audioCallback, ctx_->userData, ctx_, &ctx_->mediaAudioChannel
    );
    ctx_->mediaAudioChannel->receive(ctx_->audioEventHandler);

    std::cerr << "Media audio channel setup complete" << std::endl;

    // Create speech audio strand and channel (for navigation/assistant voice)
    ctx_->speechAudioStrand = std::make_unique<boost::asio::io_service::strand>(ctx_->ioService);
    ctx_->speechAudioChannel = std::make_shared<channel::av::SpeechAudioServiceChannel>(
        *ctx_->speechAudioStrand, ctx_->messenger
    );

    // Create and store audio event handler for speech audio, passing pointer to the channel
    ctx_->speechAudioEventHandler = std::make_shared<AudioEventHandler>(
        ctx_->audioCallback, ctx_->userData, ctx_, &ctx_->speechAudioChannel
    );
    ctx_->speechAudioChannel->receive(ctx_->speechAudioEventHandler);

    std::cerr << "Speech audio channel setup complete" << std::endl;

    // Create system audio strand and channel (for Android Auto UI sounds)
    ctx_->systemAudioStrand = std::make_unique<boost::asio::io_service::strand>(ctx_->ioService);
    ctx_->systemAudioChannel = std::make_shared<channel::av::SystemAudioServiceChannel>(
        *ctx_->systemAudioStrand, ctx_->messenger
    );

    // Create and store audio event handler for system audio, passing pointer to the channel
    ctx_->systemAudioEventHandler = std::make_shared<AudioEventHandler>(
        ctx_->audioCallback, ctx_->userData, ctx_, &ctx_->systemAudioChannel
    );
    ctx_->systemAudioChannel->receive(ctx_->systemAudioEventHandler);

    std::cerr << "System audio channel setup complete" << std::endl;

    // TODO: Set up input channel for touch/button events

    std::cerr << "Service channels ready, waiting for channel open requests..." << std::endl;

    // Continue receiving messages on control channel
    if (ctx_->controlChannel && ctx_->controlEventHandler) {
        std::cerr << "Re-registering control channel after service discovery..." << std::endl;
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
    } else {
        std::cerr << "ERROR: Control channel or handler is null after service discovery!" << std::endl;
    }

    // Debug: Log channel registration status
    std::cerr << "Channel registration status:" << std::endl;
    std::cerr << "  - Video channel: " << (ctx_->videoChannel ? "registered" : "NULL") << std::endl;
    std::cerr << "  - Media audio channel: " << (ctx_->mediaAudioChannel ? "registered" : "NULL") << std::endl;
    std::cerr << "  - Speech audio channel: " << (ctx_->speechAudioChannel ? "registered" : "NULL") << std::endl;
    std::cerr << "  - System audio channel: " << (ctx_->systemAudioChannel ? "registered" : "NULL") << std::endl;
    std::cerr << "  - Control channel: " << (ctx_->controlChannel ? "registered" : "NULL") << std::endl;

    // Set up a timer to log if we don't receive any channel open requests
    auto timeout_timer = std::make_shared<boost::asio::deadline_timer>(ctx_->ioService);
    timeout_timer->expires_from_now(boost::posix_time::seconds(5));
    timeout_timer->async_wait([](const boost::system::error_code& ec) {
        if (!ec) {
            std::cerr << "========================================" << std::endl;
            std::cerr << "WARNING: 5 seconds passed since service discovery" << std::endl;
            std::cerr << "No channel open requests received from phone yet!" << std::endl;
            std::cerr << "Phone may be showing 'incompatible software' error" << std::endl;
            std::cerr << "========================================" << std::endl;
        }
    });
}

void ControlEventHandler::onAudioFocusRequest(const proto::messages::AudioFocusRequest& request) {
    std::cerr << "Audio focus request received" << std::endl;

    if (!ctx_) {
        std::cerr << "Context is null in onAudioFocusRequest" << std::endl;
        return;
    }

    // Grant audio focus
    proto::messages::AudioFocusResponse response;
    response.set_audio_focus_state(proto::enums::AudioFocusState::GAIN);

    std::cerr << "Granting audio focus" << std::endl;

    auto promise = messenger::SendPromise::defer(ctx_->ioService);
    promise->then([]() {
        std::cerr << "Audio focus response sent" << std::endl;
    }, [](const error::Error& e) {
        std::cerr << "Failed to send audio focus response: " << e.what() << std::endl;
    });

    ctx_->controlChannel->sendAudioFocusResponse(response, std::move(promise));

    // Continue receiving messages on control channel
    if (ctx_->controlChannel && ctx_->controlEventHandler) {
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
    }
}

void ControlEventHandler::onShutdownRequest(const proto::messages::ShutdownRequest& request) {
    std::cerr << "Shutdown request received" << std::endl;
    if (ctx_) {
        ctx_->stop();
        if (ctx_->connectionCallback) {
            ctx_->connectionCallback(false, ctx_->userData);
        }
    }
}

void ControlEventHandler::onShutdownResponse(const proto::messages::ShutdownResponse& response) {
    std::cerr << "Shutdown response received" << std::endl;
    // Note: Not re-registering here since we're shutting down
}

void ControlEventHandler::onNavigationFocusRequest(const proto::messages::NavigationFocusRequest& request) {
    // TODO: Handle navigation focus

    // Continue receiving messages on control channel
    if (ctx_ && ctx_->controlChannel && ctx_->controlEventHandler) {
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
    }
}

void ControlEventHandler::onPingResponse(const proto::messages::PingResponse& response) {
    // TODO: Handle ping response

    // Continue receiving messages on control channel
    if (ctx_ && ctx_->controlChannel && ctx_->controlEventHandler) {
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
    }
}

void ControlEventHandler::onChannelError(const error::Error& e) {
    std::cerr << "========================================" << std::endl;
    std::cerr << "CONTROL CHANNEL ERROR: " << e.what() << std::endl;
    std::cerr << "Error code: " << (int)e.getCode() << ", native: " << e.getNativeCode() << std::endl;
    std::cerr << "This may indicate protocol incompatibility!" << std::endl;
    std::cerr << "========================================" << std::endl;

    // Continue receiving messages on control channel even after error
    if (ctx_ && ctx_->controlChannel && ctx_->controlEventHandler) {
        std::cerr << "Re-registering control channel after error..." << std::endl;
        ctx_->controlChannel->receive(ctx_->controlEventHandler);
    }
}

// C callback wrappers
extern "C" {

AASDKHandle aasdk_init(VideoFrameCallback video_cb, AudioDataCallback audio_cb, ConnectionStatusCallback conn_cb, void* user_data) {
    try {
        auto* ctx = new AASDKContext();
        ctx->videoCallback = video_cb;
        ctx->audioCallback = audio_cb;
        ctx->connectionCallback = conn_cb;
        ctx->userData = user_data;
        
        // Initialize libusb
        libusb_context* usbContext = nullptr;
        int ret = libusb_init(&usbContext);
        if (ret != 0) {
            std::cerr << "Failed to initialize libusb: " << libusb_error_name(ret) << std::endl;
            delete ctx;
            return nullptr;
        }
        ctx->usbContext = usbContext;
        
        // Create USB wrapper
        ctx->usbWrapper = std::make_unique<usb::USBWrapper>(usbContext);
        
        // Create query factory
        ctx->queryFactory = std::make_unique<usb::AccessoryModeQueryFactory>(
            *ctx->usbWrapper, ctx->ioService
        );
        
        // Create query chain factory
        ctx->queryChainFactory = std::make_unique<usb::AccessoryModeQueryChainFactory>(
            *ctx->usbWrapper, ctx->ioService, *ctx->queryFactory
        );
        
        // Create USB hub - must use shared_ptr because USBHub inherits from enable_shared_from_this
        ctx->usbHub = std::make_shared<usb::USBHub>(
            *ctx->usbWrapper, ctx->ioService, *ctx->queryChainFactory
        );
        
        // Start IO service thread
        ctx->work = std::make_unique<boost::asio::io_service::work>(ctx->ioService);
        ctx->running = true;
        ctx->ioThread = std::thread([ctx]() {
            try {
                while (ctx->running) {
                    // Run io_service with timeout to allow libusb event handling
                    ctx->ioService.poll();  // Process ready handlers without blocking

                    // Handle libusb events with short timeout
                    struct timeval tv = {0, 100000}; // 100ms timeout
                    libusb_handle_events_timeout_completed(ctx->usbContext, &tv, nullptr);

                    // Small sleep to prevent busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (const std::exception& e) {
                std::cerr << "IO service thread error: " << e.what() << std::endl;
            }
        });
        
        std::cerr << "AASDK initialized successfully" << std::endl;
        return static_cast<AASDKHandle>(ctx);
    } catch (const std::exception& e) {
        std::cerr << "AASDK initialization failed: " << e.what() << std::endl;
        return nullptr;
    }
}

bool aasdk_start(AASDKHandle handle) {
    if (!handle) {
        std::cerr << "Invalid AASDK handle" << std::endl;
        return false;
    }
    
    AASDKContext* ctx = static_cast<AASDKContext*>(handle);
    
    try {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        
        if (!ctx->running) {
            std::cerr << "AASDK context is not running" << std::endl;
            return false;
        }
        
        if (!ctx->usbHub) {
            std::cerr << "USBHub is not initialized" << std::endl;
            return false;
        }
        
        // Dispatch the start operation to the io_service thread to ensure proper context
        // This ensures all AASDK operations happen on the correct thread
        boost::asio::post(ctx->ioService, [ctx]() {
            try {
                // Helper function to start USBHub for hotplug events
                auto startUSBHub = [ctx]() {
                    auto promise = usb::IUSBHub::Promise::defer(ctx->ioService);
                    promise->then([ctx](usb::DeviceHandle deviceHandle) {
                        std::cerr << "USB device discovered via hotplug, setting up connection..." << std::endl;
                        setupDeviceConnection(ctx, deviceHandle);
                    }, [ctx](const error::Error& error) {
                        std::cerr << "USB discovery failed: " << error.what() << std::endl;
                        if (ctx->connectionCallback) {
                            ctx->connectionCallback(false, ctx->userData);
                        }
                    });
                    
                    ctx->usbHub->start(std::move(promise));
                    std::cerr << "USBHub started, waiting for new devices..." << std::endl;
                };
                
                // Note: In WSL2, USB hotplug events may not work properly
                // So we rely primarily on enumeration of already-connected devices
                // Start USBHub in background for hotplug (may not work in WSL2)
                std::cerr << "Starting USBHub to listen for hotplug events (may not work in WSL2)..." << std::endl;
                startUSBHub();
                
                // Enumerate already-connected devices - this is the primary method for WSL2
                std::cerr << "Enumerating already-connected devices (primary method for WSL2)..." << std::endl;

                // Add small delay to let devices stabilize after USB initialization
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                usb::DeviceListHandle deviceListHandle;
                auto listResult = ctx->usbWrapper->getDeviceList(deviceListHandle);
                
                if (listResult >= 0 && !deviceListHandle->empty()) {
                    std::cerr << "Found " << deviceListHandle->size() << " USB device(s), checking for Android Auto capable devices..." << std::endl;
                    
                    // Try each device
                    for (auto deviceIter = deviceListHandle->begin(); deviceIter != deviceListHandle->end(); ++deviceIter) {
                        // First check device descriptor to see if it's already in AOAP mode
                        libusb_device_descriptor deviceDescriptor;
                        auto descResult = ctx->usbWrapper->getDeviceDescriptor(*deviceIter, deviceDescriptor);
                        
                        if (descResult == 0) {
                            // Skip USB hubs (Linux Foundation vendor ID 0x1d6b)
                            if (deviceDescriptor.idVendor == 0x1d6b) {
                                std::cerr << "Skipping USB hub: VID=0x" << std::hex << deviceDescriptor.idVendor 
                                         << " PID=0x" << deviceDescriptor.idProduct << std::dec << std::endl;
                                continue;
                            }
                            
                            // Check if device is already in AOAP mode (Google vendor ID + AOAP product ID)
                            bool isAOAP = (deviceDescriptor.idVendor == 0x18D1) && 
                                         (deviceDescriptor.idProduct == 0x2D00 || deviceDescriptor.idProduct == 0x2D01);
                            
                            std::cerr << "Device: VID=0x" << std::hex << deviceDescriptor.idVendor 
                                     << " PID=0x" << deviceDescriptor.idProduct << std::dec
                                     << (isAOAP ? " (AOAP mode)" : "") << std::endl;
                            
                            if (isAOAP) {
                                // Device is already in AOAP mode, try to open it directly with retry logic
                                // Retry logic for initial connection (handles timing issues)
                                const int MAX_RETRIES = 3;
                                bool connected = false;

                                for (int retry = 0; retry < MAX_RETRIES; retry++) {
                                    usb::DeviceHandle deviceHandle;
                                    auto openResult = ctx->usbWrapper->open(*deviceIter, deviceHandle);

                                    if (openResult != 0 || deviceHandle == nullptr) {
                                        std::cerr << "Failed to open AOAP device: " << openResult << std::endl;
                                        if (retry < MAX_RETRIES - 1) {
                                            std::cerr << "Retrying open in 300ms..." << std::endl;
                                            std::this_thread::sleep_for(std::chrono::milliseconds(300));
                                            continue;
                                        }
                                        break;
                                    }

                                    try {
                                        if (retry > 0) {
                                            std::cerr << "Connection attempt " << (retry + 1) << " of " << MAX_RETRIES << "..." << std::endl;
                                        } else {
                                            std::cerr << "Device already in AOAP mode, setting up connection..." << std::endl;
                                        }

                                        setupDeviceConnection(ctx, deviceHandle);
                                        connected = true;
                                        std::cerr << "Successfully connected to AOAP device!" << std::endl;
                                        break; // Success
                                    } catch (const error::Error& e) {
                                        std::cerr << "Connection attempt " << (retry + 1) << " failed: " << e.what()
                                                 << " (code: " << (int)e.getCode() << ", native: " << e.getNativeCode() << ")" << std::endl;

                                        // Device handle is consumed on error, need to reopen
                                        deviceHandle.reset();

                                        if (retry < MAX_RETRIES - 1) {
                                            std::cerr << "Retrying in 500ms..." << std::endl;
                                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        }
                                    } catch (const std::exception& e) {
                                        std::cerr << "Connection attempt " << (retry + 1) << " failed: " << e.what() << std::endl;

                                        // Device handle is consumed on error, need to reopen
                                        deviceHandle.reset();

                                        if (retry < MAX_RETRIES - 1) {
                                            std::cerr << "Retrying in 500ms..." << std::endl;
                                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        }
                                    }
                                }

                                if (connected) {
                                    break; // Found and connected
                                } else {
                                    std::cerr << "All connection attempts failed, will rely on hotplug..." << std::endl;
                                }
                            } else if (deviceDescriptor.idVendor == 0x18D1) {
                                // Google device (likely Android phone) but not in AOAP mode yet
                                // Try to open and query it to switch to AOAP mode
                                usb::DeviceHandle deviceHandle;
                                auto openResult = ctx->usbWrapper->open(*deviceIter, deviceHandle);
                                
                                if (openResult == 0 && deviceHandle != nullptr) {
                                    std::cerr << "Opened Google device (VID=0x18d1 PID=0x" << std::hex << deviceDescriptor.idProduct << std::dec << ")" << std::endl;
                                    std::cerr << "Creating query chain to switch device to AOAP mode..." << std::endl;
                                    
                                    // Create query chain to switch device to AOAP mode
                                    ctx->activeQueryChain = ctx->queryChainFactory->create();
                                    auto queryPromise = usb::IAccessoryModeQueryChain::Promise::defer(ctx->ioService);
                                    
                                    // Add a timeout mechanism - if query chain takes too long, cancel it
                                    auto queryTimeout = std::make_shared<boost::asio::deadline_timer>(ctx->ioService);
                                    queryTimeout->expires_from_now(boost::posix_time::seconds(30));
                                    queryTimeout->async_wait([ctx, queryTimeout](const boost::system::error_code& ec) {
                                        if (!ec && ctx->activeQueryChain) {
                                            std::cerr << "========================================" << std::endl;
                                            std::cerr << "Query chain timeout (30s) - canceling..." << std::endl;
                                            std::cerr << "========================================" << std::endl;
                                            std::cerr << "Possible issues:" << std::endl;
                                            std::cerr << "1. Android phone may need to accept 'Allow USB accessory?' prompt" << std::endl;
                                            std::cerr << "2. USB debugging must be enabled in Developer options" << std::endl;
                                            std::cerr << "3. In WSL2, USB control transfers may not work properly" << std::endl;
                                            std::cerr << "4. Try unplugging and replugging your phone" << std::endl;
                                            std::cerr << "5. Check if Android Auto app is installed and set up" << std::endl;
                                            std::cerr << "========================================" << std::endl;
                                            ctx->activeQueryChain->cancel();
                                            ctx->activeQueryChain.reset();
                                        }
                                    });
                                    
                                    std::cerr << "Query chain steps:" << std::endl;
                                    std::cerr << "  1. PROTOCOL_VERSION" << std::endl;
                                    std::cerr << "  2. SEND_MANUFACTURER (\"Android\")" << std::endl;
                                    std::cerr << "  3. SEND_MODEL (\"Android Auto\")" << std::endl;
                                    std::cerr << "  4. SEND_DESCRIPTION (\"Android Auto\")" << std::endl;
                                    std::cerr << "  5. SEND_VERSION (\"2.0.1\")" << std::endl;
                                    std::cerr << "  6. SEND_URI (\"https://f1xstudio.com\")" << std::endl;
                                    std::cerr << "  7. SEND_SERIAL (\"HU-AAAAAA001\")" << std::endl;
                                    std::cerr << "  8. START (switch to AOAP mode)" << std::endl;
                                    std::cerr << "Watch your phone for 'Allow USB accessory?' prompt!" << std::endl;
                                    
                                    queryPromise->then([ctx, queryTimeout](usb::DeviceHandle handle) {
                                        queryTimeout->cancel(); // Cancel timeout on success
                                        std::cerr << "========================================" << std::endl;
                                        std::cerr << "Device successfully switched to AOAP mode!" << std::endl;
                                        std::cerr << "Setting up connection..." << std::endl;
                                        std::cerr << "========================================" << std::endl;
                                        ctx->activeQueryChain.reset();
                                        setupDeviceConnection(ctx, handle);
                                    }, [ctx, queryTimeout](const error::Error& e) {
                                        queryTimeout->cancel(); // Cancel timeout on error
                                        std::cerr << "========================================" << std::endl;
                                        std::cerr << "Query chain failed: " << e.what() << std::endl;
                                        std::cerr << "Error code: " << (int)e.getCode() << std::endl;
                                        std::cerr << "========================================" << std::endl;
                                        ctx->activeQueryChain.reset();
                                        // USBHub is already running in background
                                    });
                                    
                                    ctx->activeQueryChain->start(std::move(deviceHandle), std::move(queryPromise));
                                    // Don't break - let it run in background, USBHub is already started
                                    break; // Only try first Google device
                                } else {
                                    std::cerr << "Failed to open Google device (error " << openResult << "), trying next..." << std::endl;
                                }
                            } else {
                                std::cerr << "Skipping non-Google device (VID=0x" << std::hex << deviceDescriptor.idVendor << std::dec << ")" << std::endl;
                            }
                        } else {
                            std::cerr << "Failed to get device descriptor: " << descResult << std::endl;
                        }
                    }
                } else {
                    std::cerr << "No USB devices found or enumeration failed." << std::endl;
                }
                
            } catch (const std::bad_weak_ptr& e) {
                std::cerr << "AASDK start failed: bad_weak_ptr - " << e.what() << std::endl;
                if (ctx->connectionCallback) {
                    ctx->connectionCallback(false, ctx->userData);
                }
            } catch (const std::exception& e) {
                std::cerr << "AASDK start failed: " << e.what() << std::endl;
                if (ctx->connectionCallback) {
                    ctx->connectionCallback(false, ctx->userData);
                }
            }
        });
        
        std::cerr << "AASDK started, waiting for device..." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "AASDK start failed: " << e.what() << std::endl;
        return false;
    }
}

void aasdk_stop(AASDKHandle handle) {
    if (!handle) return;
    
    AASDKContext* ctx = static_cast<AASDKContext*>(handle);
    ctx->stop();
    
    if (ctx->connectionCallback) {
        ctx->connectionCallback(false, ctx->userData);
    }
}

void aasdk_deinit(AASDKHandle handle) {
    if (!handle) return;
    
    AASDKContext* ctx = static_cast<AASDKContext*>(handle);
    
    // Cleanup libusb
    if (ctx->usbContext) {
        libusb_exit(ctx->usbContext);
        ctx->usbContext = nullptr;
    }
    
    delete ctx;
}

void aasdk_send_touch_event(AASDKHandle handle, int32_t x, int32_t y, int32_t action) {
    if (!handle) return;
    
    AASDKContext* ctx = static_cast<AASDKContext*>(handle);
    
    // TODO: Implement touch event sending via InputServiceChannel
    // This requires creating input channel messages
    std::cerr << "Touch event: x=" << x << ", y=" << y << ", action=" << action << std::endl;
}

void aasdk_send_button_event(AASDKHandle handle, int32_t button_code, bool pressed) {
    if (!handle) return;
    
    AASDKContext* ctx = static_cast<AASDKContext*>(handle);
    
    // TODO: Implement button event sending via InputServiceChannel
    std::cerr << "Button event: code=" << button_code << ", pressed=" << pressed << std::endl;
}

} // extern "C"
