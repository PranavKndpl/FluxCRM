#ifndef FLUXDB_CLIENT_HPP
#define FLUXDB_CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

#include "document.hpp"
#include "query_parser.hpp" 

#pragma comment(lib, "Ws2_32.lib")

namespace fluxdb {

using Id = std::uint64_t;

class FluxDBClient {
private:
    SOCKET sock = INVALID_SOCKET;
    std::string host;
    int port;

    // Helper: Send raw string, get raw string
    std::string sendCommand(const std::string& cmd) {
        if (sock == INVALID_SOCKET) throw std::runtime_error("Not connected");

        std::string payload = cmd + "\n";
        if (send(sock, payload.c_str(), payload.length(), 0) == SOCKET_ERROR) {
            throw std::runtime_error("Send failed");
        }

        // Dynamic Buffer Strategy
        std::string response;
        char buffer[4096]; // Keep the chunk size on the stack
        
        while (true) {
            int bytes = recv(sock, buffer, sizeof(buffer), 0);
            
            // 1. Connection closed or Error
            if (bytes <= 0) break; 

            // 2. Append chunk to our total response
            response.append(buffer, bytes);

            // 3. Check for Protocol Delimiter (Newline)
            // FluxDB sends messages ending in '\n'. If we found it, we are done.
            if (response.back() == '\n') {
                response.pop_back(); // Remove the delimiter
                break;
            }
        }

        return response;
    }

public:
    // DELETE Copying
    FluxDBClient(const FluxDBClient&) = delete;
    FluxDBClient& operator=(const FluxDBClient&) = delete;

    // ENABLE Moving
    FluxDBClient(FluxDBClient&& other) noexcept : host(std::move(other.host)), port(other.port), sock(other.sock) {
        other.sock = INVALID_SOCKET; // Nullify the old one so destructor doesn't kill it
    }

    FluxDBClient(const std::string& h, int p) : host(h), port(p) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        connectToServer();
    }

    ~FluxDBClient() {
        if (sock != INVALID_SOCKET) closesocket(sock);
        WSACleanup();
    }

    FluxDBClient& operator=(FluxDBClient&& other) noexcept {
        if (this != &other) {
            // Close our current socket if open
            if (sock != INVALID_SOCKET) closesocket(sock);
            
            // Steal resources
            sock = other.sock;
            host = std::move(other.host);
            port = other.port;
            
            // Nullify source
            other.sock = INVALID_SOCKET;
        }
        return *this;
    }

    void connectToServer() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[Client] Connection failed.\n";
            sock = INVALID_SOCKET;
        } else {
            std::cout << "[Client] Connected to " << host << ":" << port << "\n";
        }
    }

    // --- API METHODS ---

    bool auth(const std::string& password) {
        std::string resp = sendCommand("AUTH " + password);
        return resp == "OK AUTHENTICATED";
    }

    bool use(const std::string& dbName) {
        std::string resp = sendCommand("USE " + dbName);
        return resp.find("OK SWITCHED_TO") == 0;
    }

    Id insert(const Document& doc) {
        
        Value v(doc); 
        std::string json = v.ToJson();
        
        std::string resp = sendCommand("INSERT " + json);

        if (resp.find("OK ID=") == 0) {
            return std::stoull(resp.substr(6));
        }
        return 0; 
    }

    bool update(Id id, const Document& doc) {
        // Wrapper to serialize document
        Value v(doc);
        std::string json = v.ToJson();
        
        // Command: UPDATE <id> <json>
        std::string resp = sendCommand("UPDATE " + std::to_string(id) + " " + json);
        return resp == "OK UPDATED";
    }

    bool remove(Id id) {
        std::string resp = sendCommand("DELETE " + std::to_string(id));
        return resp == "OK DELETED";
    }

    std::vector<Document> find(const Document& query) {
        Value v(query);
        std::string resp = sendCommand("FIND " + v.ToJson());
        
        std::vector<Document> results;
        std::stringstream ss(resp);
        std::string line;
        
        if (!std::getline(ss, line) || line.find("OK") != 0) return results;

        while (std::getline(ss, line)) {
            if (line.find("ID ") == 0) {
                size_t jsonStart = line.find('{');
                if (jsonStart != std::string::npos) {
                    std::string idStr = line.substr(3, jsonStart - 3);
                    std::string jsonStr = line.substr(jsonStart);
                    
                    // --- SAFETY & DEBUGGING ---
                    try {
                        QueryParser parser(jsonStr);
                        Document d = parser.parseJSON();
                        
                        try {
                            d["_id"] = std::make_shared<Value>(static_cast<int64_t>(std::stoull(idStr)));
                        } catch (...) {}

                        results.push_back(d);
                    } 
                    catch (const std::exception& e) {
                    
                        std::cerr << "[Client Warning] Failed to parse: [" << jsonStr << "]\n";
                        std::cerr << "   -> Error: " << e.what() << "\n";
                    }
                    
                }
            }
        }
        return results;
    }

    int publish(const std::string& channel, const std::string& message) {
        std::string cmd = "PUBLISH " + channel + " " + message;
        std::string resp = sendCommand(cmd);
        
        if (resp.find("OK RECEIVERS=") == 0) {
            try {
                return std::stoi(resp.substr(13));
            } catch (...) { return 0; }
        }
        return 0;
    }

    // Listen loop 
    void subscribe(const std::string& channel, std::function<void(const std::string&)> callback) {
        if (sock == INVALID_SOCKET) return;
        
        std::string cmd = "SUBSCRIBE " + channel + "\n";
        if (send(sock, cmd.c_str(), cmd.length(), 0) == SOCKET_ERROR) return;
        
        // Custom Read Loop for Streaming
        char buffer[4096];
        while (true) {
            int bytes = recv(sock, buffer, 4096, 0);
            if (bytes <= 0) break; // Disconnected or Error
            
            buffer[bytes] = '\0';
            std::string raw(buffer);
            
            // Format: MESSAGE <channel> <content>
            std::stringstream ss(raw);
            std::string line;
            while(std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back(); // Trim
                
                if (line.find("MESSAGE ") == 0) {
                    // Extract content (Find second space)
                    size_t firstSpace = line.find(' ');
                    size_t secondSpace = line.find(' ', firstSpace + 1);
                    
                    if (secondSpace != std::string::npos) {
                        std::string msg = line.substr(secondSpace + 1);
                        callback(msg);
                    }
                }
            }
        }
    }
    
    std::string rawCommand(const std::string& cmd) {
        return sendCommand(cmd);
    }
};

}

#endif