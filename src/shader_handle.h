#ifndef SHADER_HANDLE_H
#define SHADER_HANDLE_H

#include <cstdint>

// Opaque handle for backend shader resources
// This allows shader_system to be backend-agnostic
using ShaderHandle = uint64_t;

constexpr ShaderHandle INVALID_SHADER_HANDLE = 0;

#endif // SHADER_HANDLE_H
