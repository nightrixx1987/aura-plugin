#pragma once

// Precompiled Header für Aura — Standard-Library und System-Header
// Wird einmal kompiliert und für alle .cpp Dateien gecached
// Beschleunigt inkrementelle Builds um ~30-50%

// C++ Standard Library
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

// Windows-spezifische Header (werden von JUCE intern gebraucht)
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
#endif
