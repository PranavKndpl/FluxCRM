#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include "../crm_core.hpp"

namespace CLI {

    class CLIHost {
    private:
        CRMSystem crm;
        bool running = true;
        bool is_connected = false;

        std::vector<std::string> tokenize(const std::string& input) {
            std::vector<std::string> tokens;
            std::stringstream ss(input);
            std::string token;
            while (ss >> token) tokens.push_back(token);
            return tokens;
        }

        void printRow(const std::string& c1, const std::string& c2, const std::string& c3, const std::string& c4) {
            std::cout << "| " << std::left << std::setw(6) << c1 
                      << " | " << std::setw(20) << c2 
                      << " | " << std::setw(20) << c3 
                      << " | " << std::setw(15) << c4 << " |\n";
        }

        void exportCSV(const std::string& filename) {
            std::ofstream file(filename);
            if (!file.is_open()) { std::cout << "ERR Could not open file for writing.\n"; return; }
            
            file << "ID,Name,Company,Value,Stage\n";
            const char* stages[] = { "New", "Contacted", "Won" };
            int count = 0;
            
            for (const char* stage : stages) {
                auto leads = crm.getLeadsByStage(stage);
                for (const auto& l : leads) {
                    file << l.id << "," << l.name << "," << l.company << "," << l.value << "," << stage << "\n";
                    count++;
                }
            }
            std::cout << "OK Exported " << count << " leads to " << filename << "\n";
            file.close();
        }

        void importCSV(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) { std::cout << "ERR Could not open file " << filename << "\n"; return; }

            std::string line;
            int count = 0;
            std::getline(file, line); 

            while (std::getline(file, line)) {
                if (line.empty()) continue;
                std::stringstream ss(line);
                std::string segment;
                std::vector<std::string> row;
                while(std::getline(ss, segment, ',')) row.push_back(segment);

                if (row.size() >= 3) {
                    Lead l;
                    l.name = row[0];
                    l.company = row[1];
                    try { l.value = std::stoi(row[2]); } catch(...) { l.value = 0; }
                    if (crm.addLead(l)) count++;
                }
            }
            std::cout << "OK Imported " << count << " leads.\n";
            if (count > 0) crm.publishEvent("CLI: Imported " + std::to_string(count) + " leads from CSV");
        }

    public:
        void run() {
            std::cout << "FluxCRM (CLI Mode)\nType 'HELP' for commands.\n";

            while (running) {
                std::cout << (is_connected ? "flux> " : "(offline)> ");
                std::string line;
                if (!std::getline(std::cin, line)) break;
                if (line.empty()) continue;

                std::vector<std::string> args = tokenize(line);
                std::string cmd = args[0];
                for (auto & c: cmd) c = toupper(c);

                try {
                    if (cmd == "EXIT") running = false;

                    else if (cmd == "HELP") {
                        std::cout <<
                            "\nAvailable Commands:\n"
                            "  CONNECT <ip> <port> <pass>\n"
                            "  ADD <stage> <name> <company> <value>\n"
                            "  LIST [stage]\n"
                            "  PROMOTE <id> <stage>\n"
                            "  TASK <id> <desc>\n"
                            "  IMPORT <file.csv>\n"
                            "  EXPORT <file.csv>\n"
                            "  STATS\n"
                            "  GOAL <amount>\n"
                            "  EXIT\n\n";
                    }

                    else if (cmd == "CONNECT") {
                        if (args.size() < 4) { std::cout << "Usage: CONNECT <ip> <port> <pass>\n"; continue; }
                        if (crm.connect(args[1], std::stoi(args[2]), args[3])) {
                            is_connected = true;
                            std::cout << "OK Connected.\n";
                        } else std::cout << "ERR " << crm.getError() << "\n";
                    }

                    else if (!is_connected) { std::cout << "ERR Not connected.\n"; continue; }

                    else if (cmd == "ADD" && args.size() >= 4) {
                        Lead l;
                        l.name = args[2]; l.company = args[3]; l.value = std::stoi(args[4]);
                        if (crm.addLead(l)) {
                            std::cout << "OK Lead Created.\n";
                            crm.publishEvent("CLI: New Lead " + l.name); 
                        }
                    }

                    else if (cmd == "TASK") {
                        if (args.size() < 3) { std::cout << "Usage: TASK <id> <desc>\n"; continue; }
                        int id = std::stoi(args[1]);
                        std::string desc = "";
                        for(size_t i=2; i<args.size(); i++) desc += args[i] + " ";
                        
                        if (crm.addTask(id, desc, "")) {
                            std::cout << "OK Task Added.\n";
                            crm.publishEvent("CLI: Task added to #" + std::to_string(id));
                        }
                    }

                    else if (cmd == "PROMOTE") {
                        if (args.size() < 3) continue;
                        int id = std::stoi(args[1]);
                        std::string newStage = args[2];
                        
                        bool found = false;
                        const char* stages[] = { "New", "Contacted", "Won" };
                        Lead target;
                        for (const char* s : stages) {
                            auto leads = crm.getLeadsByStage(s);
                            for(auto& l : leads) { if(l.id == id) { target = l; found = true; break; } }
                            if(found) break;
                        }

                        if(found && crm.updateLeadStatus(target, newStage)) {
                             std::cout << "OK Promoted.\n";
                             crm.publishEvent("CLI: Promoted " + target.name + " to " + newStage);
                        } else std::cout << "ERR Failed.\n";
                    }

                    else if (cmd == "IMPORT") {
                        if (args.size() < 2) std::cout << "Usage: IMPORT <filename.csv>\n";
                        else importCSV(args[1]);
                    }

                    else if (cmd == "EXPORT") {
                        if (args.size() < 2) std::cout << "Usage: EXPORT <filename.csv>\n";
                        else exportCSV(args[1]);
                    }

                    else if (cmd == "STATS") {
                         const char* stages[] = { "New", "Contacted", "Won" };
                         std::cout << "\n=== PIPELINE HEALTH ===\n";
                         printRow("STAGE", "COUNT", "VALUE", "AVG");
                         std::cout << "+" << std::string(66, '-') << "+\n";
                         for (const char* stage : stages) {
                             auto leads = crm.getLeadsByStage(stage);
                             double val = 0; for(auto& l : leads) val += l.value;
                             double avg = leads.empty() ? 0 : val/leads.size();
                             printRow(stage, std::to_string(leads.size()), std::to_string((int)val), std::to_string((int)avg));
                         }
                         std::cout << "\n";
                    }

                    else if (cmd == "LIST") {
                        std::string stage = (args.size() > 1) ? args[1] : "New";
                        auto leads = crm.getLeadsByStage(stage);
                        printRow("ID", "NAME", "COMPANY", "VALUE");
                        std::cout << "--------------------------------------------------------\n";
                        for(auto& l : leads) printRow(std::to_string(l.id), l.name, l.company, std::to_string(l.value));
                        std::cout << "Total: " << leads.size() << "\n";
                    }

                    else if (cmd == "GOAL") {
                        if (args.size() < 2) { 
                            std::cout << "Usage: GOAL <amount>\n";
                            continue; 
                        }
                        try {
                            int amount = std::stoi(args[1]);
                            if (crm.setPerformanceGoal(amount)) {
                                std::cout << "OK Performance goal updated to $" << amount << "\n";
                                crm.publishEvent("CLI: Goal updated to $" + std::to_string(amount));
                            } else {
                                std::cout << "ERR Failed to update goal.\n";
                            }
                        } catch (...) { std::cout << "ERR Invalid number.\n"; }
                    }

                    else {
                        std::cout << "ERR Unknown command '" << cmd << "'. Type HELP.\n";
                    }

                } catch (const std::exception& e) { std::cout << "ERR " << e.what() << "\n"; }
            }
        }
    };
}
