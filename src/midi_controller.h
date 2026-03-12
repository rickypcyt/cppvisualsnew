#ifndef MIDI_CONTROLLER_H
#define MIDI_CONTROLLER_H

#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

struct MidiMessage {
    int channel;
    int control;
    int value;
    double timestamp;
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
    
private:
    void processMidiInput();
    void handleMidiMessage(const MidiMessage& message);
    
    std::atomic<bool> running_;
    std::thread midiThread_;
    mutable std::mutex controlMutex_;
    
    std::vector<std::function<void(float)>> controlCallbacks_;
    std::vector<float> controlValues_;
    std::vector<bool> controlActive_;
    static constexpr int MAX_CONTROLS = 128;
    
    // Platform-specific MIDI handle
    void* midiHandle_;
    int midiPort_;
    bool initialized_;
};

#endif // MIDI_CONTROLLER_H
