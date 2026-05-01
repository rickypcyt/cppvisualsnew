#include "midi_controller.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

using json = nlohmann::json;

MidiController::MidiController() 
    : running_(false), midiHandle_(nullptr), midiPort_(-1), initialized_(false),
      totalMessages_(0), noteOnCount_(0), noteOffCount_(0), controlChangeCount_(0),
      pitchBendCount_(0), otherCount_(0), lastActivityTime_(0.0),
      midiLoggingEnabled_(false) {
    controlValues_.resize(MAX_CONTROLS, 0.0f);
    controlActive_.resize(MAX_CONTROLS, false);
    controlCallbacks_.resize(MAX_CONTROLS);

    noteCallbacks_.resize(MAX_CHANNELS);
    for (auto& channelCallbacks : noteCallbacks_) {
        channelCallbacks.resize(MAX_NOTES);
    }

    messageHistory_.reserve(MAX_MESSAGE_HISTORY);
    statsStartTime_ = std::chrono::steady_clock::now();
}

MidiController::~MidiController() {
    disableMIDILogging();  // Close log file if open
    shutdown();
}

bool MidiController::initialize() {
#ifdef __linux__
    
    snd_seq_t* seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        std::cerr << "Failed to open ALSA sequencer" << std::endl;
        return false;
    }
    
    snd_seq_set_client_name(seq, "AudioVisualizer");
    
    int port = snd_seq_create_simple_port(seq, "Input",
                                         SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                         SND_SEQ_PORT_TYPE_APPLICATION);
    
    if (port < 0) {
        std::cerr << "Failed to create ALSA port" << std::endl;
        snd_seq_close(seq);
        return false;
    }
    
    midiHandle_ = seq;
    midiPort_ = port;
    initialized_ = true;
    
    std::cout << "MIDI controller initialized successfully" << std::endl;
    std::cout << "Client ID: " << snd_seq_client_id(seq) << ", Port: " << port << std::endl;
    
    // List available MIDI devices
    auto devices = getAvailableDevices();
    std::cout << "Available MIDI devices (" << devices.size() << "):" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "  " << i << ": " << devices[i] << std::endl;
    }
    
    return true;
#else
    std::cerr << "MIDI support not implemented for this platform" << std::endl;
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

void MidiController::bindNote(int channel, int note, std::function<void()> callback) {
    if (channel >= 0 && channel < MAX_CHANNELS && note >= 0 && note < MAX_NOTES) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        noteCallbacks_[channel][note] = callback;
    }
}

void MidiController::unbindNote(int channel, int note) {
    if (channel >= 0 && channel < MAX_CHANNELS && note >= 0 && note < MAX_NOTES) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        noteCallbacks_[channel][note] = nullptr;
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
        std::cerr << "Cannot start MIDI - not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Starting MIDI processing thread..." << std::endl;
    running_ = true;
    midiThread_ = std::thread(&MidiController::processMidiInput, this);
    
    std::cout << "MIDI processing started" << std::endl;
    return true;
}

void MidiController::stop() {
    running_ = false;
    if (midiThread_.joinable()) {
        midiThread_.join();
        std::cout << "MIDI thread stopped" << std::endl;
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
            if (client != 0) { // Skip system client
                devices.push_back(snd_seq_client_info_get_name(clientInfo));
            }
        }
        
        snd_seq_close(seq);
    }
#endif
    
    return devices;
}

void MidiController::processMidiInput() {
#ifdef __linux__
    
    snd_seq_t* seq = static_cast<snd_seq_t*>(midiHandle_);
    if (!seq) {
        std::cerr << "Invalid MIDI handle in processing thread" << std::endl;
        return;
    }
    
    int npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    if (npfd <= 0) {
        std::cerr << "No MIDI poll descriptors available" << std::endl;
        return;
    }
    
    struct pollfd* pfd = (struct pollfd*)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN);
    
    
    while (running_) {
        if (poll(pfd, npfd, 100) > 0) {  // 100ms timeout
            snd_seq_event_t* ev = nullptr;
            int eventCount = 0;
            
            do {
                if (snd_seq_event_input(seq, &ev) >= 0) {
                    eventCount++;
                    MidiMessage msg;
                    msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    
                    if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
                        msg.type = MidiEventType::CONTROL_CHANGE;
                        msg.channel = ev->data.control.channel;
                        msg.data1 = ev->data.control.param;
                        msg.data2 = ev->data.control.value;
                        
                        
                        handleMidiMessage(msg);
                    } else if (ev->type == SND_SEQ_EVENT_NOTEON) {
                        msg.type = MidiEventType::NOTE_ON;
                        msg.channel = ev->data.note.channel;
                        msg.data1 = ev->data.note.note;
                        msg.data2 = ev->data.note.velocity;

                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_NOTEOFF) {
                        msg.type = MidiEventType::NOTE_OFF;
                        msg.channel = ev->data.note.channel;
                        msg.data1 = ev->data.note.note;
                        msg.data2 = ev->data.note.velocity;

                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_PITCHBEND) {
                        msg.type = MidiEventType::PITCH_BEND;
                        msg.channel = ev->data.control.channel;
                        msg.data1 = ev->data.control.value & 0x7F;  // LSB
                        msg.data2 = (ev->data.control.value >> 7) & 0x7F;  // MSB

                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_CHANPRESS) {
                        msg.type = MidiEventType::CHANNEL_PRESSURE;
                        msg.channel = ev->data.control.channel;
                        msg.data1 = ev->data.control.value;
                        msg.data2 = 0;
                        
                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_PGMCHANGE) {
                        msg.type = MidiEventType::PROGRAM_CHANGE;
                        msg.channel = ev->data.control.channel;
                        msg.data1 = ev->data.control.value;
                        msg.data2 = 0;
                        
                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_KEYPRESS) {
                        msg.type = MidiEventType::POLY_AFTERTOUCH;
                        msg.channel = ev->data.note.channel;
                        msg.data1 = ev->data.note.note;
                        msg.data2 = ev->data.note.velocity;
                        
                        addMessageToHistory(msg);
                    } else if (ev->type == SND_SEQ_EVENT_SYSEX) {
                        msg.type = MidiEventType::SYSEX;
                        msg.channel = 0;
                        msg.data1 = 0;
                        msg.data2 = 0;
                        
                        addMessageToHistory(msg);
                    } else {
                        msg.type = MidiEventType::UNKNOWN;
                        msg.channel = 0;
                        msg.data1 = 0;
                        msg.data2 = 0;
                        
                        addMessageToHistory(msg);
                    }
                }
            } while (snd_seq_event_input_pending(seq, 0) > 0);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Reduced sleep for better responsiveness
    }
    
    std::cout << "[MIDI DEBUG] MIDI input processing thread ended" << std::endl;
#endif
}

void MidiController::handleMidiMessage(const MidiMessage& message) {
    addMessageToHistory(message);
    
    // Log to file if enabled
    if (midiLoggingEnabled_) {
        std::lock_guard<std::mutex> lock(logMutex_);
        if (midiLogFile_.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            if (message.type == MidiEventType::CONTROL_CHANGE) {
                midiLogFile_ << "CC Ch" << message.channel 
                           << " CC" << message.data1 
                           << " Value:" << message.data2 
                           << " Time:" << now_ms << "\n";
                midiLogFile_.flush();
            } else if (message.type == MidiEventType::NOTE_ON) {
                midiLogFile_ << "NOTE_ON Ch" << message.channel 
                           << " Note:" << message.data1 
                           << " Vel:" << message.data2 
                           << " Time:" << now_ms << "\n";
                midiLogFile_.flush();
            } else if (message.type == MidiEventType::NOTE_OFF) {
                midiLogFile_ << "NOTE_OFF Ch" << message.channel 
                           << " Note:" << message.data1 
                           << " Vel:" << message.data2 
                           << " Time:" << now_ms << "\n";
                midiLogFile_.flush();
            }
        }
    }
    
    if (message.type == MidiEventType::CONTROL_CHANGE && message.data1 >= 0 && message.data1 < MAX_CONTROLS) {
        float normalizedValue = static_cast<float>(message.data2) / 127.0f;

        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            controlValues_[message.data1] = normalizedValue;
            controlActive_[message.data1] = true;
        }

        std::cout << "CC " << message.data1 << " -> " << normalizedValue << std::endl;

        // Call bound callback if exists
        if (controlCallbacks_[message.data1]) {
            controlCallbacks_[message.data1](normalizedValue);
        }
    }

    // Handle NOTE ON events with note callbacks
    if (message.type == MidiEventType::NOTE_ON && message.channel >= 0 && message.channel < MAX_CHANNELS && message.data1 >= 0 && message.data1 < MAX_NOTES) {
        std::lock_guard<std::mutex> lock(controlMutex_);
        if (noteCallbacks_[message.channel][message.data1]) {
            std::cout << "NOTE callback: Ch" << message.channel << " Note" << message.data1 << std::endl;
            noteCallbacks_[message.channel][message.data1]();
        } else {
            std::cout << "NOTE no callback: Ch" << message.channel << " Note" << message.data1 << std::endl;
        }
    }
}

void MidiController::addMessageToHistory(const MidiMessage& message) {
    // Update statistics
    totalMessages_++;
    lastActivityTime_ = message.timestamp;
    
    switch (message.type) {
        case MidiEventType::NOTE_ON:
            noteOnCount_++;
            break;
        case MidiEventType::NOTE_OFF:
            noteOffCount_++;
            break;
        case MidiEventType::CONTROL_CHANGE:
            controlChangeCount_++;
            break;
        case MidiEventType::PITCH_BEND:
            pitchBendCount_++;
            break;
        default:
            otherCount_++;
            break;
    }
    
    // Add to message history
    {
        std::lock_guard<std::mutex> lock(messageHistoryMutex_);
        messageHistory_.push_back(message);
        
        // Keep only recent messages
        while (messageHistory_.size() > MAX_MESSAGE_HISTORY) {
            messageHistory_.erase(messageHistory_.begin());
        }
    }
}

std::vector<MidiMessage> MidiController::getMessageHistory(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(messageHistoryMutex_);
    
    if (maxCount == 0 || messageHistory_.size() <= maxCount) {
        return messageHistory_;
    }
    
    // Return the most recent maxCount messages
    return std::vector<MidiMessage>(
        messageHistory_.end() - maxCount,
        messageHistory_.end()
    );
}

MidiController::MidiStatistics MidiController::getStatistics() const {
    MidiStatistics stats;
    stats.totalMessages = totalMessages_.load();
    stats.noteOnCount = noteOnCount_.load();
    stats.noteOffCount = noteOffCount_.load();
    stats.controlChangeCount = controlChangeCount_.load();
    stats.pitchBendCount = pitchBendCount_.load();
    stats.otherCount = otherCount_.load();
    stats.lastActivityTime = lastActivityTime_.load();
    
    // Calculate messages per second
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - statsStartTime_).count();
    stats.messagesPerSecond = elapsed > 0 ? stats.totalMessages / static_cast<int>(elapsed) : 0;
    
    return stats;
}

void MidiController::clearMessageHistory() {
    std::lock_guard<std::mutex> lock(messageHistoryMutex_);
    messageHistory_.clear();
    
    // Reset statistics
    totalMessages_ = 0;
    noteOnCount_ = 0;
    noteOffCount_ = 0;
    controlChangeCount_ = 0;
    pitchBendCount_ = 0;
    otherCount_ = 0;
    statsStartTime_ = std::chrono::steady_clock::now();
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

// Dynamic MIDI mapping implementation
void MidiController::setMapping(const std::string& parameterName, int ccNumber, bool isToggle, float minVal, float maxVal) {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    MidiMapping mapping;
    mapping.parameterName = parameterName;
    mapping.ccNumber = ccNumber;
    mapping.isToggle = isToggle;
    mapping.minValue = minVal;
    mapping.maxValue = maxVal;
    
    midiMappings_[parameterName] = mapping;
    ccToParameterMap_[ccNumber] = parameterName;
    
    std::cout << "[MIDI MAPPING] Set " << parameterName << " -> CC " << ccNumber << std::endl;
}

MidiController::MidiMapping MidiController::getMapping(const std::string& parameterName) const {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    auto it = midiMappings_.find(parameterName);
    if (it != midiMappings_.end()) {
        return it->second;
    }
    
    return MidiMapping{"", -1, false, 0.0f, 1.0f};
}

void MidiController::removeMapping(const std::string& parameterName) {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    auto it = midiMappings_.find(parameterName);
    if (it != midiMappings_.end()) {
        int ccNumber = it->second.ccNumber;
        ccToParameterMap_.erase(ccNumber);
        midiMappings_.erase(it);
        std::cout << "[MIDI MAPPING] Removed mapping for " << parameterName << std::endl;
    }
}

std::vector<MidiController::MidiMapping> MidiController::getAllMappings() const {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    std::vector<MidiMapping> mappings;
    for (const auto& [name, mapping] : midiMappings_) {
        mappings.push_back(mapping);
    }
    
    return mappings;
}

void MidiController::clearAllMappings() {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    midiMappings_.clear();
    ccToParameterMap_.clear();
    std::cout << "[MIDI MAPPING] Cleared all mappings" << std::endl;
}

bool MidiController::loadMappingsFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[MIDI MAPPING] Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        midiMappings_.clear();
        ccToParameterMap_.clear();
        
        for (const auto& item : j.items()) {
            MidiMapping mapping;
            mapping.parameterName = item.key();
            mapping.ccNumber = item.value()["cc"];
            mapping.isToggle = item.value().value("isToggle", false);
            mapping.minValue = item.value().value("minValue", 0.0f);
            mapping.maxValue = item.value().value("maxValue", 1.0f);
            
            midiMappings_[mapping.parameterName] = mapping;
            ccToParameterMap_[mapping.ccNumber] = mapping.parameterName;
        }
        
        std::cout << "[MIDI MAPPING] Loaded " << midiMappings_.size() << " mappings from " << filename << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI MAPPING] Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }
}

bool MidiController::saveMappingsToFile(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    json j;
    for (const auto& [name, mapping] : midiMappings_) {
        j[name] = {
            {"cc", mapping.ccNumber},
            {"isToggle", mapping.isToggle},
            {"minValue", mapping.minValue},
            {"maxValue", mapping.maxValue}
        };
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[MIDI MAPPING] Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    file << j.dump(4);
    std::cout << "[MIDI MAPPING] Saved " << midiMappings_.size() << " mappings to " << filename << std::endl;
    return true;
}

std::string MidiController::getParameterForCC(int ccNumber) const {
    std::lock_guard<std::mutex> lock(mappingMutex_);
    
    auto it = ccToParameterMap_.find(ccNumber);
    if (it != ccToParameterMap_.end()) {
        return it->second;
    }
    
    return "";
}

void MidiController::enableMIDILogging(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (midiLogFile_.is_open()) {
        midiLogFile_.close();
    }
    
    midiLogFilename_ = filename;
    midiLogFile_.open(filename, std::ios::app);
    
    std::cout << "[MIDI LOG] Attempting to open file: " << filename << std::endl;
    
    if (midiLogFile_.is_open()) {
        midiLoggingEnabled_ = true;
        
        // Write header
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        midiLogFile_ << "\n========== MIDI LOG STARTED ==========\n";
        midiLogFile_ << "Time: " << std::ctime(&now_time_t);
        midiLogFile_ << "====================================\n";
        midiLogFile_.flush();
        
        std::cout << "[MIDI LOG] Logging enabled to: " << filename << std::endl;
    } else {
        std::cerr << "[MIDI LOG] Failed to open log file: " << filename << std::endl;
        std::cerr << "[MIDI LOG] Error: " << strerror(errno) << std::endl;
    }
}

void MidiController::disableMIDILogging() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (midiLogFile_.is_open()) {
        midiLogFile_ << "\n========== MIDI LOG STOPPED ==========\n";
        midiLogFile_.flush();
        midiLogFile_.close();
    }
    
    midiLoggingEnabled_ = false;
}

bool MidiController::isMIDILoggingEnabled() const {
    return midiLoggingEnabled_;
}
