#ifndef IPC_VISUAL_STATE_H
#define IPC_VISUAL_STATE_H

#include <string>
#include <nlohmann/json.hpp>

struct VisualState {
    int proceduralMode = 1;
    float intensity = 1.0f;
    struct Fx {
        float bloom = 0.5f;
        float chromatic = 0.2f;
    } fx;
    bool postFXEnabled = true;
};

inline void to_json(nlohmann::json& j, const VisualState& state) {
    j = nlohmann::json{
        {"visual", {
            {"mode", state.proceduralMode},
            {"intensity", state.intensity},
            {"postFX", {
                {"bloom", state.fx.bloom},
                {"chromatic", state.fx.chromatic},
                {"enabled", state.postFXEnabled}
            }}
        }}
    };
}

inline void from_json(const nlohmann::json& j, VisualState& state) {
    if (j.contains("visual")) {
        auto visual = j.at("visual");
        visual.value("mode", state.proceduralMode);
        visual.value("intensity", state.intensity);
        if (visual.contains("postFX")) {
            auto post = visual.at("postFX");
            post.value("bloom", state.fx.bloom);
            post.value("chromatic", state.fx.chromatic);
            post.value("enabled", state.postFXEnabled);
        }
    }
}

inline VisualState parseVisualState(const std::string& message) {
    VisualState state;
    try {
        auto j = nlohmann::json::parse(message);
        state = j.get<VisualState>();
    } catch (const std::exception&) {
        // Keep defaults on parse failure
    }
    return state;
}

inline std::string serializeVisualState(const VisualState& state) {
    return nlohmann::json(state).dump();
}

#endif // IPC_VISUAL_STATE_H
