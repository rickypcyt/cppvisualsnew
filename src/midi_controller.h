#ifndef MIDI_CONTROLLER_H
#define MIDI_CONTROLLER_H

#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <fstream>

enum class MidiEventType {
    NOTE_ON,
    NOTE_OFF,
    CONTROL_CHANGE,
    PITCH_BEND,
    CHANNEL_PRESSURE,
    PROGRAM_CHANGE,
    POLY_AFTERTOUCH,
    SYSEX,
    UNKNOWN
};

struct MidiMessage {
    MidiEventType type;
    int channel;
    int data1;  // Control number or note number
    int data2;  // Value or velocity
    double timestamp;
    
    // For pitch bend (data1 = LSB, data2 = MSB)
    int getPitchBendValue() const {
        return (data2 << 7) | data1;
    }
};

class MidiController {
public:
    MidiController();
    ~MidiController();
    
    bool initialize();
    void shutdown();
    
    // MIDI control mappings
    void bindControl(int control, std::function<void(float)> callback);
    void unbindControl(int control);

    // MIDI note bindings
    void bindNote(int channel, int note, std::function<void()> callback);
    void unbindNote(int channel, int note);

    // Get current control values
    float getControlValue(int control) const;
    bool isControlActive(int control) const;
    
    // Start/stop MIDI processing
    bool start();
    void stop();
    
    // Get list of available MIDI devices
    std::vector<std::string> getAvailableDevices() const;
    
    // Auto-connect all available MIDI devices
    void connectAllDevices();
    
    // Message history and statistics
    struct MidiStatistics {
        int totalMessages;
        int messagesPerSecond;
        int noteOnCount;
        int noteOffCount;
        int controlChangeCount;
        int pitchBendCount;
        int otherCount;
        double lastActivityTime;
    };
    
    std::vector<MidiMessage> getMessageHistory(size_t maxCount = 100) const;
    MidiStatistics getStatistics() const;
    void clearMessageHistory();
    
    // Dynamic MIDI mapping system
    struct MidiMapping {
        std::string parameterName;
        int ccNumber;
        bool isToggle;
        float minValue;
        float maxValue;
    };
    
    void setMapping(const std::string& parameterName, int ccNumber, bool isToggle = false, float minVal = 0.0f, float maxVal = 1.0f);
    MidiMapping getMapping(const std::string& parameterName) const;
    void removeMapping(const std::string& parameterName);
    std::vector<MidiMapping> getAllMappings() const;
    void clearAllMappings();
    
    bool loadMappingsFromFile(const std::string& filename);
    bool saveMappingsToFile(const std::string& filename) const;
    
    // Get parameter name mapped to a specific CC
    std::string getParameterForCC(int ccNumber) const;
    
    // MIDI logging
    void enableMIDILogging(const std::string& filename);
    void disableMIDILogging();
    bool isMIDILoggingEnabled() const;
    
private:
    void processMidiInput();
    void handleMidiMessage(const MidiMessage& message);
    void addMessageToHistory(const MidiMessage& message);
    
    std::atomic<bool> running_;
    std::thread midiThread_;
    mutable std::mutex controlMutex_;
    
    std::vector<std::function<void(float)>> controlCallbacks_;
    std::vector<float> controlValues_;
    std::vector<bool> controlActive_;
    static constexpr int MAX_CONTROLS = 128;

    // Note callbacks: indexed by [channel][note]
    std::vector<std::vector<std::function<void()>>> noteCallbacks_;
    static constexpr int MAX_CHANNELS = 16;
    static constexpr int MAX_NOTES = 128;
    
    // Message history
    mutable std::mutex messageHistoryMutex_;
    std::vector<MidiMessage> messageHistory_;
    static constexpr size_t MAX_MESSAGE_HISTORY = 500;
    
    // Statistics
    std::atomic<int> totalMessages_;
    std::atomic<int> noteOnCount_;
    std::atomic<int> noteOffCount_;
    std::atomic<int> controlChangeCount_;
    std::atomic<int> pitchBendCount_;
    std::atomic<int> otherCount_;
    std::atomic<double> lastActivityTime_;
    std::chrono::steady_clock::time_point statsStartTime_;
    
    // Platform-specific MIDI handle
    void* midiHandle_;
    int midiPort_;
    bool initialized_;
    
    // Dynamic MIDI mappings (parameter name -> CC mapping)
    mutable std::mutex mappingMutex_;
    std::unordered_map<std::string, MidiMapping> midiMappings_;
    
    // Reverse mapping (CC -> parameter name) for quick lookup
    std::unordered_map<int, std::string> ccToParameterMap_;
    
    // MIDI logging
    mutable std::mutex logMutex_;
    std::ofstream midiLogFile_;
    std::string midiLogFilename_;
    std::atomic<bool> midiLoggingEnabled_;
};

#endif // MIDI_CONTROLLER_H
