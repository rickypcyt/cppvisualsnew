#include "midi_controller.h"
#include <iostream>
#include <chrono>
#include <algorithm>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

MidiController::MidiController() 
    : running_(false), midiHandle_(nullptr), midiPort_(-1), initialized_(false) {
    controlValues_.resize(MAX_CONTROLS, 0.0f);
    controlActive_.resize(MAX_CONTROLS, false);
    controlCallbacks_.resize(MAX_CONTROLS);
}

MidiController::~MidiController() {
    shutdown();
}

bool MidiController::initialize() {
#ifdef __linux__
    std::cout << "[MIDI DEBUG] Initializing MIDI controller..." << std::endl;
    
    snd_seq_t* seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        std::cerr << "[MIDI DEBUG] Failed to open ALSA sequencer" << std::endl;
        return false;
    }
    
    snd_seq_set_client_name(seq, "AudioVisualizer");
    
    int port = snd_seq_create_simple_port(seq, "Input",
                                         SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                         SND_SEQ_PORT_TYPE_APPLICATION);
    
    if (port < 0) {
        std::cerr << "[MIDI DEBUG] Failed to create ALSA port" << std::endl;
        snd_seq_close(seq);
        return false;
    }
    
    midiHandle_ = seq;
    midiPort_ = port;
    initialized_ = true;
    
    std::cout << "[MIDI DEBUG] MIDI controller initialized successfully" << std::endl;
    std::cout << "[MIDI DEBUG] Client ID: " << snd_seq_client_id(seq) << ", Port: " << port << std::endl;
    
    // List available MIDI devices
    auto devices = getAvailableDevices();
    std::cout << "[MIDI DEBUG] Available MIDI devices (" << devices.size() << "):" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "[MIDI DEBUG]   " << i << ": " << devices[i] << std::endl;
    }
    
    return true;
#else
    std::cerr << "[MIDI DEBUG] MIDI support not implemented for this platform" << std::endl;
    return false;
#endif
}

void MidiController::shutdown() {
    stop();
    
    if (initialized_ && midiHandle_) {
#ifdef __linux__
        snd_seq_t* seq = static_cast<snd_seq_t*>(midiHandle_);
        snd_seq_close(seq);
#endif
        midiHandle_ = nullptr;
        initialized_ = false;
    }
}

void MidiController::bindControl(int control, std::function<void(float)> callback) {
    if (control >= 0 && control < MAX_CONTROLS) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlCallbacks_[control] = callback;
        controlActive_[control] = true;
    }
}

void MidiController::unbindControl(int control) {
    if (control >= 0 && control < MAX_CONTROLS) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        controlCallbacks_[control] = nullptr;
        controlActive_[control] = false;
    }
}

float MidiController::getControlValue(int control) const {
    if (control >= 0 && control < MAX_CONTROLS) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        return controlValues_[control];
    }
    return 0.0f;
}

bool MidiController::isControlActive(int control) const {
    if (control >= 0 && control < MAX_CONTROLS) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        return controlActive_[control];
    }
    return false;
}

bool MidiController::start() {
    if (!initialized_) {
        std::cerr << "[MIDI DEBUG] Cannot start MIDI - not initialized" << std::endl;
        return false;
    }
    
    std::cout << "[MIDI DEBUG] Starting MIDI processing thread..." << std::endl;
    running_ = true;
    midiThread_ = std::thread(&MidiController::processMidiInput, this);
    
    std::cout << "[MIDI DEBUG] MIDI processing started" << std::endl;
    return true;
}

void MidiController::stop() {
    std::cout << "[MIDI DEBUG] Stopping MIDI processing..." << std::endl;
    running_ = false;
    if (midiThread_.joinable()) {
        midiThread_.join();
        std::cout << "[MIDI DEBUG] MIDI thread stopped" << std::endl;
    }
}

std::vector<std::string> MidiController::getAvailableDevices() const {
    std::vector<std::string> devices;
    
#ifdef __linux__
    snd_seq_t* seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) >= 0) {
        snd_seq_client_info_t* clientInfo;
        snd_seq_client_info_alloca(&clientInfo);
        
        snd_seq_client_info_set_client(clientInfo, -1);
        while (snd_seq_query_next_client(seq, clientInfo) >= 0) {
            int client = snd_seq_client_info_get_client(clientInfo);
            const char* clientName = snd_seq_client_info_get_name(clientInfo);
            
            if (client == 0) continue; // Skip system client
            
            // Check if this client has MIDI output ports
            snd_seq_port_info_t* portInfo;
            snd_seq_port_info_alloca(&portInfo);
            snd_seq_port_info_set_client(portInfo, client);
            snd_seq_port_info_set_port(portInfo, -1);
            
            bool hasMidiPort = false;
            while (snd_seq_query_next_port(seq, portInfo) >= 0) {
                unsigned int capability = snd_seq_port_info_get_capability(portInfo);
                // Check if this port can send MIDI
                if ((capability & SND_SEQ_PORT_CAP_READ) && 
                    (capability & SND_SEQ_PORT_CAP_SUBS_READ)) {
                    hasMidiPort = true;
                    const char* portName = snd_seq_port_info_get_name(portInfo);
                    std::string deviceName = std::string(clientName) + " - " + portName;
                    devices.push_back(deviceName);
                }
            }
            
            // If no specific ports found but client exists, add client name
            if (!hasMidiPort) {
                devices.push_back(clientName);
            }
        }
        
        snd_seq_close(seq);
    }
#endif
    
    return devices;
}

void MidiController::processMidiInput() {
#ifdef __linux__
    std::cout << "[MIDI DEBUG] Starting MIDI input processing thread..." << std::endl;
    
    snd_seq_t* seq = static_cast<snd_seq_t*>(midiHandle_);
    if (!seq) {
        std::cerr << "[MIDI DEBUG] Invalid MIDI handle in processing thread" << std::endl;
        return;
    }
    
    int npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    if (npfd <= 0) {
        std::cerr << "[MIDI DEBUG] No MIDI poll descriptors available" << std::endl;
        return;
    }
    
    struct pollfd* pfd = (struct pollfd*)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN);
    
    std::cout << "[MIDI DEBUG] MIDI poll descriptors initialized, waiting for input..." << std::endl;
    
    while (running_) {
        if (poll(pfd, npfd, 100) > 0) {  // 100ms timeout
            snd_seq_event_t* ev = nullptr;
            int eventCount = 0;
            
            do {
                if (snd_seq_event_input(seq, &ev) >= 0) {
                    eventCount++;
                    std::cout << "[MIDI DEBUG] Received MIDI event type: " << ev->type 
                              << " (count: " << eventCount << ")" << std::endl;
                    
                    if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
                        MidiMessage msg;
                        msg.channel = ev->data.control.channel;
                        msg.control = ev->data.control.param;
                        msg.value = ev->data.control.value;
                        msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        
                        std::cout << "[MIDI DEBUG] MIDI CC event - Channel: " << (int)msg.channel 
                                  << ", CC: " << (int)msg.control << ", Value: " << (int)msg.value << std::endl;
                        
                        handleMidiMessage(msg);
                    } else {
                        const char* typeName = "UNKNOWN";
                        switch (ev->type) {
                            case SND_SEQ_EVENT_NOTEON: typeName = "NOTE ON"; break;
                            case SND_SEQ_EVENT_NOTEOFF: typeName = "NOTE OFF"; break;
                            case SND_SEQ_EVENT_PITCHBEND: typeName = "PITCH BEND"; break;
                            case SND_SEQ_EVENT_CHANPRESS: typeName = "CHANNEL PRESSURE"; break;
                            case SND_SEQ_EVENT_PGMCHANGE: typeName = "PROGRAM CHANGE"; break;
                            case SND_SEQ_EVENT_KEYPRESS: typeName = "POLY AFTERTOUCH"; break;
                            case SND_SEQ_EVENT_SYSEX: typeName = "SYSEX"; break;
                            case SND_SEQ_EVENT_QFRAME: typeName = "MTC QUARTER FRAME"; break;
                            case SND_SEQ_EVENT_TICK: typeName = "TICK"; break;
                            case SND_SEQ_EVENT_TEMPO: typeName = "TEMPO"; break;
                            case SND_SEQ_EVENT_SONGPOS: typeName = "SONG POSITION"; break;
                            case SND_SEQ_EVENT_SONGSEL: typeName = "SONG SELECT"; break;
                            default: break;
                        }

                        std::cout << "[MIDI DEBUG] Non-CC MIDI event type: " << typeName
                                  << " (" << ev->type << ")" << std::endl;
                        if (ev->type == SND_SEQ_EVENT_NOTEON || ev->type == SND_SEQ_EVENT_NOTEOFF) {
                            std::cout << "[MIDI DEBUG]   Channel: " << static_cast<int>(ev->data.note.channel)
                                      << ", Note: " << static_cast<int>(ev->data.note.note)
                                      << ", Velocity: " << static_cast<int>(ev->data.note.velocity) << std::endl;
                        }
                    }
                }
            } while (snd_seq_event_input_pending(seq, 0) > 0);
            
            if (eventCount > 0) {
                std::cout << "[MIDI DEBUG] Processed " << eventCount << " MIDI events this cycle" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Reduced sleep for better responsiveness
    }
    
    std::cout << "[MIDI DEBUG] MIDI input processing thread ended" << std::endl;
#endif
}

void MidiController::handleMidiMessage(const MidiMessage& message) {
    std::cout << "[MIDI DEBUG] Received MIDI message - Channel: " << message.channel 
              << ", Control: " << message.control << ", Value: " << message.value 
              << ", Timestamp: " << message.timestamp << std::endl;
    
    if (message.control >= 0 && message.control < MAX_CONTROLS) {
        float normalizedValue = static_cast<float>(message.value) / 127.0f;
        
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            controlValues_[message.control] = normalizedValue;
        }
        
        std::cout << "[MIDI DEBUG] CC " << message.control << " -> " << normalizedValue << std::endl;
        
        // Call bound callback if exists
        if (controlCallbacks_[message.control]) {
            std::cout << "[MIDI DEBUG] Triggering callback for CC " << message.control << std::endl;
            controlCallbacks_[message.control](normalizedValue);
        } else {
            std::cout << "[MIDI DEBUG] No callback registered for CC " << message.control << std::endl;
        }
    } else {
        std::cout << "[MIDI DEBUG] CC " << message.control << " out of range (0-" << MAX_CONTROLS-1 << ")" << std::endl;
    }
}

void MidiController::connectAllDevices() {
#ifdef __linux__
    if (!initialized_ || !midiHandle_) {
        std::cerr << "[MIDI DEBUG] Cannot auto-connect MIDI devices: controller not initialized" << std::endl;
        return;
    }

    snd_seq_t* seq = static_cast<snd_seq_t*>(midiHandle_);
    int ourClientId = snd_seq_client_id(seq);
    int ourPortId = midiPort_;

    if (ourClientId < 0 || ourPortId < 0) {
        std::cerr << "[MIDI DEBUG] Invalid client/port for auto-connect (client: " << ourClientId
                  << ", port: " << ourPortId << ")" << std::endl;
        return;
    }

    std::cout << "[MIDI DEBUG] Auto-connecting MIDI devices to client " << ourClientId
              << " port " << ourPortId << std::endl;

    snd_seq_client_info_t* clientInfo;
    snd_seq_client_info_alloca(&clientInfo);
    snd_seq_client_info_set_client(clientInfo, -1);

    while (snd_seq_query_next_client(seq, clientInfo) >= 0) {
        int client = snd_seq_client_info_get_client(clientInfo);
        const char* clientName = snd_seq_client_info_get_name(clientInfo);

        // Skip system clients (0) and ourselves
        if (client == 0 || client == ourClientId) {
            continue;
        }

        std::cout << "[MIDI DEBUG] Inspecting client " << client << " - " << clientName << std::endl;

        snd_seq_port_info_t* portInfo;
        snd_seq_port_info_alloca(&portInfo);
        snd_seq_port_info_set_client(portInfo, client);
        snd_seq_port_info_set_port(portInfo, -1);

        while (snd_seq_query_next_port(seq, portInfo) >= 0) {
            unsigned int capability = snd_seq_port_info_get_capability(portInfo);
            int port = snd_seq_port_info_get_port(portInfo);
            const char* portName = snd_seq_port_info_get_name(portInfo);

            bool canSendMidi = (capability & SND_SEQ_PORT_CAP_READ) &&
                               (capability & SND_SEQ_PORT_CAP_SUBS_READ) &&
                               !(capability & SND_SEQ_PORT_CAP_NO_EXPORT);

            if (!canSendMidi) {
                continue;
            }

            std::cout << "[MIDI DEBUG] Attempting connection from " << clientName << ":" << portName
                      << " (client " << client << " port " << port << ")" << std::endl;

            snd_seq_addr_t sender{};
            sender.client = static_cast<unsigned char>(std::clamp(client, 0, 255));
            sender.port = static_cast<unsigned char>(std::clamp(port, 0, 255));

            snd_seq_addr_t dest{};
            dest.client = static_cast<unsigned char>(std::clamp(ourClientId, 0, 255));
            dest.port = static_cast<unsigned char>(std::clamp(ourPortId, 0, 255));

            snd_seq_port_subscribe_t* subs;
            snd_seq_port_subscribe_alloca(&subs);
            snd_seq_port_subscribe_set_sender(subs, &sender);
            snd_seq_port_subscribe_set_dest(subs, &dest);

            int result = snd_seq_subscribe_port(seq, subs);
            if (result == 0) {
                std::cout << "[MIDI DEBUG] Connected " << clientName << ":" << portName
                          << " -> AudioVisualizer" << std::endl;
            } else if (result == -EALREADY) {
                std::cout << "[MIDI DEBUG] Already connected to " << clientName << ":" << portName << std::endl;
            } else {
                std::cerr << "[MIDI DEBUG] Failed to connect to " << clientName << ":" << portName
                          << " (error " << result << ")" << std::endl;
            }
        }
    }

    std::cout << "[MIDI DEBUG] Auto-connect process finished" << std::endl;
#endif
}
