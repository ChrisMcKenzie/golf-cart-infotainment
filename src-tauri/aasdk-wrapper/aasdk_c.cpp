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

// Event handlers that forward to C callbacks
class VideoEventHandler : public channel::av::IVideoServiceChannelEventHandler {
public:
    VideoEventHandler(VideoFrameCallback cb, void* ud) : callback_(cb), user_data_(ud) {}
    
    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) override {
        // TODO: Handle channel open
    }
    
    void onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) override {
        // TODO: Handle setup request
    }
    
    void onAVChannelStartIndication(const proto::messages::AVChannelStartIndication& indication) override {
        // TODO: Handle start indication
    }
    
    void onAVChannelStopIndication(const proto::messages::AVChannelStopIndication& indication) override {
        // TODO: Handle stop indication
    }
    
    void onAVMediaWithTimestampIndication(messenger::Timestamp::ValueType timestamp, const common::DataConstBuffer& buffer) override {
        if (callback_ && buffer.cdata) {
            // Extract video dimensions from buffer or protocol
            uint32_t width = 1280;  // Default, will be set during setup
            uint32_t height = 720;
            uint32_t stride = width * 4; // RGBA
            
            callback_(buffer.cdata, width, height, stride, user_data_);
        }
    }
    
    void onAVMediaIndication(const common::DataConstBuffer& buffer) override {
        // Similar to timestamp version
        if (callback_ && buffer.cdata) {
            uint32_t width = 1280;
            uint32_t height = 720;
            uint32_t stride = width * 4;
            callback_(buffer.cdata, width, height, stride, user_data_);
        }
    }
    
    void onVideoFocusRequest(const proto::messages::VideoFocusRequest& request) override {
        // TODO: Handle focus request
    }
    
    void onChannelError(const error::Error& e) override {
        std::cerr << "Video channel error: " << e.what() << std::endl;
    }

private:
    VideoFrameCallback callback_;
    void* user_data_;
};

class AudioEventHandler : public channel::av::IAudioServiceChannelEventHandler {
public:
    AudioEventHandler(AudioDataCallback cb, void* ud) : callback_(cb), user_data_(ud) {}
    
    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) override {}
    void onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest& request) override {}
    void onAVChannelStartIndication(const proto::messages::AVChannelStartIndication& indication) override {}
    void onAVChannelStopIndication(const proto::messages::AVChannelStopIndication& indication) override {}
    
    void onAVMediaWithTimestampIndication(messenger::Timestamp::ValueType timestamp, const common::DataConstBuffer& buffer) override {
        if (callback_ && buffer.cdata) {
            // Audio is typically 16-bit PCM
            const int16_t* samples = reinterpret_cast<const int16_t*>(buffer.cdata);
            uint32_t sample_count = buffer.size / sizeof(int16_t);
            uint32_t channels = 2; // Stereo
            uint32_t sample_rate = 48000; // Default Android Auto rate
            
            callback_(samples, sample_count, channels, sample_rate, user_data_);
        }
    }
    
    void onAVMediaIndication(const common::DataConstBuffer& buffer) override {
        if (callback_ && buffer.cdata) {
            const int16_t* samples = reinterpret_cast<const int16_t*>(buffer.cdata);
            uint32_t sample_count = buffer.size / sizeof(int16_t);
            uint32_t channels = 2;
            uint32_t sample_rate = 48000;
            callback_(samples, sample_count, channels, sample_rate, user_data_);
        }
    }
    
    void onChannelError(const error::Error& e) override {
        std::cerr << "Audio channel error: " << e.what() << std::endl;
    }

private:
    AudioDataCallback callback_;
    void* user_data_;
};

// Forward declaration
struct AASDKContext;

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
    messenger::Messenger::Pointer messenger;
    messenger::MessageInStream::Pointer messageInStream;
    messenger::MessageOutStream::Pointer messageOutStream;
    
    channel::av::VideoServiceChannel::Pointer videoChannel;
    channel::av::AudioServiceChannel::Pointer mediaAudioChannel;
    channel::av::AudioServiceChannel::Pointer speechAudioChannel;
    channel::av::AudioServiceChannel::Pointer systemAudioChannel;
    channel::input::InputServiceChannel::Pointer inputChannel;
    channel::control::ControlServiceChannel::Pointer controlChannel;
    
    std::unique_ptr<VideoEventHandler> videoEventHandler;
    std::unique_ptr<AudioEventHandler> audioEventHandler;
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

// Helper function to set up device connection
static void setupDeviceConnection(AASDKContext* ctx, usb::DeviceHandle deviceHandle) {
    try {
        std::cerr << "Setting up device connection..." << std::endl;
        
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
        
        // Create cryptor
        auto cryptor = std::make_shared<messenger::Cryptor>(sslWrapper);
        cryptor->init();
        
        // Create message streams
        ctx->messageInStream = std::make_shared<messenger::MessageInStream>(
            ctx->ioService, ctx->transport, cryptor
        );
        ctx->messageOutStream = std::make_shared<messenger::MessageOutStream>(
            ctx->ioService, ctx->transport, cryptor
        );
        
        // Create messenger
        ctx->messenger = std::make_shared<messenger::Messenger>(
            ctx->ioService, ctx->messageInStream, ctx->messageOutStream
        );
        
        // Create control channel
        boost::asio::io_service::strand controlStrand(ctx->ioService);
        ctx->controlChannel = std::make_shared<channel::control::ControlServiceChannel>(
            controlStrand, ctx->messenger
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
}

void ControlEventHandler::onHandshake(const common::DataConstBuffer& payload) {
    std::cerr << "Handshake received" << std::endl;
    // TODO: Complete handshake and set up service channels
}

void ControlEventHandler::onServiceDiscoveryRequest(const proto::messages::ServiceDiscoveryRequest& request) {
    std::cerr << "Service discovery request received" << std::endl;
    // TODO: Respond with available services
}

void ControlEventHandler::onAudioFocusRequest(const proto::messages::AudioFocusRequest& request) {
    // TODO: Handle audio focus
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
}

void ControlEventHandler::onNavigationFocusRequest(const proto::messages::NavigationFocusRequest& request) {
    // TODO: Handle navigation focus
}

void ControlEventHandler::onPingResponse(const proto::messages::PingResponse& response) {
    // TODO: Handle ping response
}

void ControlEventHandler::onChannelError(const error::Error& e) {
    std::cerr << "Control channel error: " << e.what() << std::endl;
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
                    ctx->ioService.run();
                    if (ctx->running) {
                        ctx->ioService.reset();
                    }
                    libusb_handle_events_completed(ctx->usbContext, nullptr);
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
                                // Device is already in AOAP mode, try to open it directly
                                usb::DeviceHandle deviceHandle;
                                auto openResult = ctx->usbWrapper->open(*deviceIter, deviceHandle);
                                
                                if (openResult == 0 && deviceHandle != nullptr) {
                                    std::cerr << "Device already in AOAP mode, setting up connection..." << std::endl;
                                    setupDeviceConnection(ctx, deviceHandle);
                                    break; // Found and connected
                                } else {
                                    std::cerr << "Failed to open AOAP device: " << openResult << std::endl;
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
