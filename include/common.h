#pragma once

// Common includes used throughout the project
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <exception>
#include <iostream>

// Forward declarations
namespace sherpa_onnx { 
    namespace cxx {
        class OfflineRecognizer;
        class VoiceActivityDetector;
        class OfflineStream;
        struct OfflineRecognizerResult;
        struct SpeechSegment;
    }
}

// WebSocket related
typedef struct websocketpp_connection_hdl_tag* connection_hdl_type;
#define connection_hdl connection_hdl_type
