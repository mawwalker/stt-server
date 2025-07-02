#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection_hdl connection_hdl;

// Custom hash function for connection_hdl
struct ConnectionHdlHash {
    std::size_t operator()(const connection_hdl& hdl) const {
        return std::hash<void*>()(hdl.lock().get());
    }
};

// Custom equality function for connection_hdl
struct ConnectionHdlEqual {
    bool operator()(const connection_hdl& lhs, const connection_hdl& rhs) const {
        return !std::owner_less<std::weak_ptr<void>>()(lhs, rhs) && 
               !std::owner_less<std::weak_ptr<void>>()(rhs, lhs);
    }
};

class ConnectionManager {
private:
    std::unordered_map<connection_hdl, std::string, ConnectionHdlHash, ConnectionHdlEqual> connection_to_id;
    std::unordered_map<std::string, connection_hdl> id_to_connection;
    mutable std::mutex connections_mutex;
    std::atomic<size_t> connection_counter{0};
    std::random_device rd;
    std::mt19937 gen;
    
    std::string generate_client_id() {
        std::uniform_int_distribution<> dis(1000, 9999);
        std::stringstream ss;
        ss << "client_" << std::setfill('0') << std::setw(6) 
           << connection_counter.fetch_add(1) << "_" << dis(gen);
        return ss.str();
    }
    
public:
    ConnectionManager() : gen(rd()) {}
    
    std::string add_connection(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        std::string client_id = generate_client_id();
        connection_to_id[hdl] = client_id;
        id_to_connection[client_id] = hdl;
        return client_id;
    }
    
    void remove_connection(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = connection_to_id.find(hdl);
        if (it != connection_to_id.end()) {
            std::string client_id = it->second;
            connection_to_id.erase(it);
            id_to_connection.erase(client_id);
        }
    }
    
    std::string get_client_id(connection_hdl hdl) const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = connection_to_id.find(hdl);
        return (it != connection_to_id.end()) ? it->second : "unknown";
    }
    
    connection_hdl get_connection(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = id_to_connection.find(client_id);
        return (it != id_to_connection.end()) ? it->second : connection_hdl();
    }
    
    size_t get_connection_count() const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        return connection_to_id.size();
    }
    
    std::vector<std::string> get_all_client_ids() const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        std::vector<std::string> ids;
        ids.reserve(connection_to_id.size());
        for (const auto& pair : connection_to_id) {
            ids.push_back(pair.second);
        }
        return ids;
    }
};
