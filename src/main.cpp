// Includes
#include "../vendor/fluxdb/fluxdb_client.hpp" // Winsock first!
#include "gui_host.hpp"
#include <vector>

// --- DATA MODELS ---
struct Lead {
    int id;
    std::string name;
    std::string status; // Lead, Negotiation, Won
    int value;
};

// --- APP LOGIC ---
int main(int, char**) {
    // 1. Setup Window in one line
    CRM::AppHost app(L"FluxCRM Pro", 1280, 800);

    // 2. App State
    static char server_ip[128] = "127.0.0.1";
    static int server_port = 8080;
    static char password[128] = "flux_admin";
    
    // Use pointer or unique_ptr to control lifecycle manually
    static std::unique_ptr<fluxdb::FluxDBClient> db;
    static bool is_connected = false;
    static std::string status_msg = "Disconnected";
    
    // --- MAIN LOOP ---
    while (app.NewFrame()) {
        
        // --- SIDEBAR ---
        ImGui::Begin("Connection", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowSize(ImVec2(300, 800));
        ImGui::SetWindowPos(ImVec2(0, 0));

        ImGui::InputText("IP", server_ip, 128);
        ImGui::InputInt("Port", &server_port);
        ImGui::InputText("Pass", password, 128, ImGuiInputTextFlags_Password);

        if (ImGui::Button("Connect to FluxDB", ImVec2(280, 30))) {
            try {

                db = std::make_unique<fluxdb::FluxDBClient>(server_ip, server_port);
                
                if (db->auth(password)) {
                    if (db->use("crm_db")) {
                        is_connected = true;
                        status_msg = "Online";
                    } else {
                        status_msg = "DB Error";
                    }
                } else {
                    status_msg = "Auth Failed";
                }
            } catch (...) {
                status_msg = "Connection Failed";
                is_connected = false;
            }
        }
        
        ImGui::TextColored(is_connected ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "Status: %s", status_msg.c_str());
        ImGui::End();

        // --- KANBAN BOARD ---
        if (is_connected) {
            ImGui::SetNextWindowPos(ImVec2(300, 0));
            ImGui::SetNextWindowSize(ImVec2(980, 800));
            ImGui::Begin("Pipeline", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

            ImGui::Columns(3, "pipeline_cols");
            ImGui::Separator();
            
            const char* stages[] = { "Lead", "Negotiation", "Won" };
            
            for (int i = 0; i < 3; i++) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", stages[i]);
                ImGui::Separator();

                fluxdb::Document query;
                query["status"] = std::make_shared<fluxdb::Value>(stages[i]);
                
                auto results = db->find(query);
                
                for (const auto& doc : results) {
                    std::string name = "Unknown";
                    int val = 0;
                    
                    if (doc.count("name")) name = doc.at("name")->asString();
                    if (doc.count("value")) val = doc.at("value")->asInt();

                    ImGui::PushID(name.c_str()); // Ensure buttons have unique IDs
                    
                    // Card Styling
                    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(i * 0.35f, 0.6f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(i * 0.35f, 0.7f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(i * 0.35f, 0.8f, 0.8f));
                    
                    std::string label = name + "\n$" + std::to_string(val);
                    ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 60));
                    
                    // Context Menu 
                    if (ImGui::BeginPopupContextItem()) {
                        ImGui::Text("Actions");
                        if (ImGui::Selectable("View Details")) {}
                        ImGui::EndPopup();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                    
                    // Spacing between cards
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                }
                
                ImGui::NextColumn();
            }
            
            ImGui::End();
        }

        app.Render();
    }

    return 0;
}