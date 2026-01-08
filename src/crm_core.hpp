#ifndef CRM_CORE_HPP
#define CRM_CORE_HPP

#include "../vendor/fluxdb/fluxdb_client.hpp"

#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <ctime>

// --- DATA MODELS ---

struct Lead {
    fluxdb::Id id = 0;
    std::string name;
    std::string company;
    std::string status;
    int value = 0;
};

struct Task {
    int id = 0;
    int parent_id = 0;
    std::string description;
    bool is_done = false;
    std::string due_date;
};

struct Interaction {
    int id = 0;
    int parent_id = 0;
    std::string note;
};

// --- CONTROLLER ---

class CRMSystem {
private:
    std::unique_ptr<fluxdb::FluxDBClient> db;
    std::string last_error;

    std::string getToday() {
        time_t now = time(nullptr);
        tm local{};
        local = *localtime(&now);

        char buf[11];
        strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
        return std::string(buf);
    }

public:
    // --- CONNECTION ---

    bool connect(const std::string& ip, int port, const std::string& pass) {
        try {
            db = std::make_unique<fluxdb::FluxDBClient>(ip, port);

            if (!db->auth(pass)) {
                last_error = "Auth Failed";
                return false;
            }

            if (!db->use("crm_db")) {
                last_error = "DB Init Failed";
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            last_error = e.what();
            return false;
        }
    }

    bool isConnected() const { return db != nullptr; }
    std::string getError() const { return last_error; }

    // --- LEADS ---

    bool addLead(const Lead& lead) {
        if (!db) return false;

        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("lead");
            doc["name"] = std::make_shared<fluxdb::Value>(lead.name);
            doc["company"] = std::make_shared<fluxdb::Value>(lead.company);
            doc["status"] = std::make_shared<fluxdb::Value>("New");
            doc["value"] = std::make_shared<fluxdb::Value>((int64_t)lead.value);

            db->insert(doc);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool moveLead(fluxdb::Id id, const std::string& newStage) {
        if (!db) return false;

        const char* stages[] = { "New", "Contacted", "Won" };
        Lead foundLead;
        bool found = false;

        for (const char* stage : stages) {
            std::vector<Lead> leads = getLeadsByStage(stage);
            for (const auto& l : leads) {
                // compare uint64_t to uint64_t. Safe.
                if (l.id == id) {
                    foundLead = l;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (found) {
            if (foundLead.status != newStage) {
                return updateLeadStatus(foundLead, newStage);
            }
        }
        return false;
    }

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
                if (doc.count("_id"))     l.id = doc.at("_id")->asInt();
                if (doc.count("name"))    l.name = doc.at("name")->asString();
                if (doc.count("company")) l.company = doc.at("company")->asString();
                if (doc.count("value"))   l.value = (int)doc.at("value")->asInt();
                l.status = stage;
                output.push_back(l);
            }
        } catch (...) {}

        return output;
    }

    bool updateLeadStatus(const Lead& lead, const std::string& newStatus) {
        if (!db) return false;

        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("lead");
            doc["name"] = std::make_shared<fluxdb::Value>(lead.name);
            doc["company"] = std::make_shared<fluxdb::Value>(lead.company);
            doc["value"] = std::make_shared<fluxdb::Value>((int64_t)lead.value);
            doc["status"] = std::make_shared<fluxdb::Value>(newStatus);

            return db->update(lead.id, doc);
        } catch (...) {
            return false;
        }
    }

    bool deleteLead(int id) {
        if (!db) return false;
        return db->remove(id);
    }

    double getWonRevenue() {
        if (!db) return 0.0;

        double total = 0.0;

        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("lead");
            query["status"] = std::make_shared<fluxdb::Value>("Won");

            auto results = db->find(query);
            for (const auto& doc : results) {
                if (doc.count("value")) {
                    total += doc.at("value")->asInt();
                }
            }
        } catch (...) {}

        return total;
    }

    // --- EVENTS ---

    void publishEvent(const std::string& msg) {
        if (db) db->publish("crm_events", msg);
    }

    // --- TASKS ---

    bool addTask(int lead_id, const std::string& desc, const std::string& date) {
        if (!db) return false;

        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("task");
            doc["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            doc["description"] = std::make_shared<fluxdb::Value>(desc);
            doc["due_date"] = std::make_shared<fluxdb::Value>(date);
            doc["done"] = std::make_shared<fluxdb::Value>(false);

            db->insert(doc);
            return true;
        } catch (...) {
            return false;
        }
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
                if (doc.count("due_date")) t.due_date = doc.at("due_date")->asString();
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
        } catch (...) {
            return false;
        }
    }

    std::vector<Task> getOverdueTasks() {
        std::vector<Task> overdue;
        if (!db) return overdue;

        std::string today = getToday();

        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("task");
            query["done"] = std::make_shared<fluxdb::Value>(false);

            auto results = db->find(query);
            for (const auto& doc : results) {
                if (!doc.count("due_date")) continue;

                std::string due = doc.at("due_date")->asString();
                if (!due.empty() && due < today) {
                    Task t;
                    if (doc.count("_id")) t.id = doc.at("_id")->asInt();
                    if (doc.count("description")) t.description = doc.at("description")->asString();
                    t.due_date = due;
                    overdue.push_back(t);
                }
            }
        } catch (...) {}

        return overdue;
    }

    // --- INTERACTIONS ---

    bool addInteraction(int lead_id, const std::string& note) {
        if (!db) return false;

        try {
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("interaction");
            doc["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            doc["note"] = std::make_shared<fluxdb::Value>(note);
            db->insert(doc);
            return true;
        } catch (...) {
            return false;
        }
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
                if (doc.count("note")) i.note = doc.at("note")->asString();
                i.parent_id = lead_id;
                list.push_back(i);
            }
        } catch (...) {}

        return list;
    }
    int clearInteractions(int lead_id = -1) {
        if (!db) return 0; 
        int count = 0;

        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("interaction");
            
            if (lead_id != -1) {
                query["parent_id"] = std::make_shared<fluxdb::Value>((int64_t)lead_id);
            }

            auto results = db->find(query);

            for (const auto& doc : results) {
                if (doc.count("_id")) {
                    fluxdb::Id id = doc.at("_id")->asInt();
                    if (db->remove(id)) {
                        count++;
                    }
                }
            }
            
            if (count > 0) {
                std::string msg = "System: Cleared " + std::to_string(count) + " interaction logs.";
                publishEvent(msg); 
            }

        } catch (...) {
            return 0;
        }

        return count;
    }

    // --- CONFIGURATION ---

    bool setPerformanceGoal(int amount) {
        if (!db) return false;
        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("config");
            query["key"] = std::make_shared<fluxdb::Value>("goal");
            
            auto results = db->find(query);
            
            fluxdb::Document doc;
            doc["type"] = std::make_shared<fluxdb::Value>("config");
            doc["key"] = std::make_shared<fluxdb::Value>("goal");
            doc["val"] = std::make_shared<fluxdb::Value>((int64_t)amount);

            if (!results.empty()) {
                if (results[0].count("_id")) {
                    fluxdb::Id id = results[0].at("_id")->asInt();
                    return db->update(id, doc);
                }
            } else {
                db->insert(doc);
                return true;
            }
        } catch (...) { return false; }
        return false;
    }

    double getPerformanceGoal() {
        if (!db) return 10000.0; 

        try {
            fluxdb::Document query;
            query["type"] = std::make_shared<fluxdb::Value>("config");
            query["key"] = std::make_shared<fluxdb::Value>("goal");

            auto results = db->find(query);
            if (!results.empty() && results[0].count("val")) {
                return (double)results[0].at("val")->asInt();
            }
        } catch (...) {}
        
        return 10000.0; // default if not found
    }
};

// --- EVENT TICKER ---

class EventTicker {
private:
    std::deque<std::string> logs;
    std::mutex lock;
    std::atomic<bool> running{false};
    std::thread worker;

    std::string ip;
    int port = 0;
    std::string password;

    void listenLoop() {
        try {
            fluxdb::FluxDBClient subClient(ip, port);

            if (!password.empty()) {
                subClient.auth(password);
            }

            subClient.subscribe("crm_events", [this](const std::string& msg) {
                if (!running) return;

                std::lock_guard<std::mutex> lk(lock);
                logs.push_back(msg);
                if (logs.size() > 50) logs.pop_front();
            });
        } catch (...) {}
    }

public:
    ~EventTicker() { stop(); }

    void start(const std::string& server_ip, int server_port, const std::string& pass) {
        if (running) return;

        running = true;
        ip = server_ip;
        port = server_port;
        password = pass;

        worker = std::thread(&EventTicker::listenLoop, this);
    }

    void stop() {
        running = false;
        if (worker.joinable()) worker.detach();
    }

    std::vector<std::string> getLogs() {
        std::lock_guard<std::mutex> lk(lock);
        return {logs.begin(), logs.end()};
    }

    void clear() {
        std::lock_guard<std::mutex> lk(lock);
        logs.clear();
    }

    
};

#endif
