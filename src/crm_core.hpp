#ifndef CRM_CORE_HPP
#define CRM_CORE_HPP

#include "../vendor/fluxdb/fluxdb_client.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic> 
#include <functional>


// --- 1. DATA MODELS ---
// Plain Old Data (POD) structs. No logic.
struct Lead {
    fluxdb::Id id = 0;
    std::string name;
    std::string company;
    std::string status; // "New", "Contacted", "Won"
    int value = 0;
};

struct Task {
    int id = 0;
    int parent_id = 0; // Links to Lead
    std::string description;
    bool is_done = false;
};

struct Interaction {
    int id = 0;
    int parent_id = 0;
    std::string note;
    std::string date; // Simple string for now
};

// --- 2. CONTROLLER ---
class CRMSystem {
private:
    std::unique_ptr<fluxdb::FluxDBClient> db;
    std::string last_error;

public:
    // Connect logic is hidden here
    bool connect(const std::string& ip, int port, const std::string& pass) {
        try {
            db = std::make_unique<fluxdb::FluxDBClient>(ip, port);
            if (!db->auth(pass)) {
                last_error = "Authentication Failed";
                return false;
            }
            if (!db->use("crm_db")) {
                last_error = "Could not open crm_db";
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            last_error = e.what();
            return false;
        }
    }

    std::string getError() const { return last_error; }
    bool isConnected() const { return db != nullptr; }

    // --- LOGIC: ADD LEAD ---
    bool addLead(const Lead& lead) {
        if (!db) return false;
        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("lead"); // Discriminator
            doc["name"] = std::make_shared<fluxdb::Value>(lead.name);
            doc["company"] = std::make_shared<fluxdb::Value>(lead.company);
            doc["status"] = std::make_shared<fluxdb::Value>("New");
            doc["value"] = std::make_shared<fluxdb::Value>((int64_t)lead.value);
            
            db->insert(doc);
            return true;
        } catch (const std::exception& e) {
            last_error = e.what();
            return false;
        }
    }

    // --- LOGIC: FETCH PIPELINE ---
    // Returns a vector of Leads safely (no crashes!)
    std::vector<Lead> getLeadsByStage(const std::string& stage) {
        std::vector<Lead> output;
        if (!db) return output;

        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("lead");
            query["status"] = std::make_shared<fluxdb::Value>(stage);

            auto results = db->find(query);

            for (const auto& doc : results) {
                Lead l;
                // Safe Extraction: Check if fields exist before reading
                if (doc.count("_id"))     l.id = doc.at("_id")->asInt();
                if (doc.count("name"))    l.name = doc.at("name")->asString();
                if (doc.count("company")) l.company = doc.at("company")->asString();
                if (doc.count("value"))   l.value = (int)doc.at("value")->asInt();
                l.status = stage;
                output.push_back(l);
            }
        } catch (const std::exception& e) {
            std::cerr << "[CRM Core] Fetch error: " << e.what() << "\n";
        }
        return output;
    }

    bool updateLeadStatus(int id, const std::string& newStatus) {
        if (!db) return false;
        try {
            return false; // Placeholder until you update signature
        } catch (...) { return false; }
    }

    bool updateLeadStatus(const Lead& lead, const std::string& newStatus) {
        if (!db) return false;
        try {
            fluxdb::Document doc;
            // Reconstruct the FULL document
            doc["type"] = std::make_shared<fluxdb::Value>("lead"); // RESTORE TYPE!
            doc["name"] = std::make_shared<fluxdb::Value>(lead.name);
            doc["company"] = std::make_shared<fluxdb::Value>(lead.company);
            doc["value"] = std::make_shared<fluxdb::Value>((int64_t)lead.value);
            
            // Apply Change
            doc["status"] = std::make_shared<fluxdb::Value>(newStatus);
            
            // Send Full Update
            return db->update(lead.id, doc);
        } catch (const std::exception& e) {
            last_error = e.what();
            return false;
        }
    }

    bool deleteLead(int id) {
        if (!db) return false;
        try {
            // This sends: DELETE <id>
            return db->remove(id);
        } catch (const std::exception& e) {
            last_error = e.what();
            return false;
        }
    }

    void publishEvent(const std::string& msg) {
        if (db) db->publish("crm_events", msg);
    }




    // --- TASKS ---
    bool addTask(int lead_id, const std::string& desc) {
        if (!db) return false;
        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("task");
            doc["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            doc["description"] = std::make_shared<fluxdb::Value>(desc);
            doc["done"] = std::make_shared<fluxdb::Value>(false);
            db->insert(doc);
            return true;
        } catch (...) { return false; }
    }

    std::vector<Task> getTasks(int lead_id) {
        std::vector<Task> list;
        if (!db) return list;
        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("task");
            query["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            
            auto results = db->find(query);
            for (const auto& doc : results) {
                Task t;
                if (doc.count("_id")) t.id = doc.at("_id")->asInt();
                t.parent_id = lead_id;
                if (doc.count("description")) t.description = doc.at("description")->asString();
                if (doc.count("done")) t.is_done = doc.at("done")->asBool();
                list.push_back(t);
            }
        } catch (...) {}
        return list;
    }

    bool toggleTask(int task_id, bool new_state) {
        if (!db) return false;
        try {
            fluxdb::Document doc;
            doc["done"] = std::make_shared<fluxdb::Value>(new_state);
            return db->update(task_id, doc);
        } catch (...) { return false; }
    }

    // --- INTERACTIONS ---
    bool addInteraction(int lead_id, const std::string& note) {
        if (!db) return false;
        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("interaction");
            doc["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            doc["note"] = std::make_shared<fluxdb::Value>(note);
            // In a real app, add "timestamp" here
            db->insert(doc);
            return true;
        } catch (...) { return false; }
    }

    std::vector<Interaction> getInteractions(int lead_id) {
        std::vector<Interaction> list;
        if (!db) return list;
        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("interaction");
            query["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            
            auto results = db->find(query);
            for (const auto& doc : results) {
                Interaction i;
                if (doc.count("_id")) i.id = doc.at("_id")->asInt();
                i.parent_id = lead_id;
                if (doc.count("note")) i.note = doc.at("note")->asString();
                list.push_back(i);
            }
        } catch (...) {}
        return list;
    }
};

class EventTicker {
private:
    std::deque<std::string> logs;
    std::mutex lock;
    std::atomic<bool> running{false};
    std::thread worker;
    
    std::string ip;
    int port;
    std::string password;

    void listenLoop() {
        try {
            // Create a dedicated client for subscription
            fluxdb::FluxDBClient subClient(ip, port);
            
            if (!password.empty()) {
                if (!subClient.auth(password)) {
                    std::cerr << "[Ticker] Auth failed in background thread!\n";
                    return; 
                }
            } 

            // Subscribe logic
            // The C++ client 'subscribe' callback is blocking loop
            subClient.subscribe("crm_events", [this](const std::string& msg) {
                if (!running) return;
                
                std::lock_guard<std::mutex> lk(lock);
                logs.push_back(msg);
                if (logs.size() > 50) logs.pop_front(); // Keep last 50 logs
            });
            
        } catch (...) {
            // Connection lost
        }
    }

public:
    ~EventTicker() { stop(); }

    void start(const std::string& server_ip, int server_port, const std::string& pass) {
        if (running) return;
        running = true;
        ip = server_ip;
        port = server_port;
        password = pass; // Store it
        worker = std::thread(&EventTicker::listenLoop, this);
    }

    void stop() {
        running = false;
        // In a real blocking socket, we can't easily kill the thread unless we close the socket from outside.
        // For V1, we detach or let it hang on exit. Ideally, client.close() breaks the loop.
        if (worker.joinable()) worker.detach(); 
    }

    std::vector<std::string> getLogs() {
        std::lock_guard<std::mutex> lk(lock);
        return std::vector<std::string>(logs.begin(), logs.end());
    }
    
    // Helper to send events from the main thread
    void broadcast(fluxdb::FluxDBClient* db, const std::string& msg) {
        if (db) db->publish("crm_events", msg);
    }
};

#endif