#pragma once

#include <string>
#include <vector>
#include <json/json.h>

struct ASRResult {
    std::string text;
    bool finished;
    int idx;
    std::string lang;
    std::string emotion;
    std::string event;
    std::vector<float> timestamps;
    std::vector<std::string> tokens;
    
    Json::Value to_json() const {
        Json::Value result;
        result["text"] = text;
        result["finished"] = finished;
        result["idx"] = idx;
        result["lang"] = lang;
        result["emotion"] = emotion;
        result["event"] = event;
        
        Json::Value timestamps_array(Json::arrayValue);
        for (float timestamp : timestamps) {
            timestamps_array.append(timestamp);
        }
        result["timestamps"] = timestamps_array;
        
        Json::Value tokens_array(Json::arrayValue);
        for (const std::string& token : tokens) {
            tokens_array.append(token);
        }
        result["tokens"] = tokens_array;
        
        return result;
    }
};
