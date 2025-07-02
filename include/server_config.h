#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>

namespace ServerConfig {

const int PORT = 8080;  // Port for the WebSocket server
const int MAX_CONNECTIONS = 100;  // Maximum number of concurrent connections
const std::string AUDIO_MODEL_PATH = "./models/audio_model.onnx";  // Path to the audio model
const std::string VAD_MODEL_PATH = "./models/vad_model.onnx";  // Path to the voice activity detection model
const int BUFFER_SIZE = 4096;  // Size of the audio buffer for processing
const int SAMPLE_RATE = 16000;  // Sample rate for audio processing

}  // namespace ServerConfig

#endif  // SERVER_CONFIG_H