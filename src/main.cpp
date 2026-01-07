// --- INCLUDES ---
#include "../vendor/fluxdb/fluxdb_client.hpp" // Networking First
#include "gui_host.hpp"                       // DirectX/ImGui Boilerplate
#include "crm_core.hpp"                       // Logic Layer (CRMSystem + Ticker)
#include "../vendor/implot/implot.h"          // Graphing Library
#include <vector>

// --- APP LOGIC ---
int main(int, char**) {
    // 1. Setup Window & Contexts
    CRM::AppHost app(L"FluxCRM Pro", 1280, 900);
    
    // Initialize ImPlot Context (Required for graphs)
    ImPlot::CreateContext();

    // 2. App State
    static char server_ip[128] = "127.0.0.1";
    static int server_port = 8080;
    static char password[128] = "flux_admin";
    
    // Modal State
    static char new_lead_name[128] = "";
    static int new_lead_value = 1000;
    static char new_lead_company[128] = "";

    // Details Modal State
    static bool show_details = false;
    static Lead selected_lead; 
    static char new_task_desc[128] = "";
    static char new_note_text[256] = "";

    // Controller & Systems
    static CRMSystem crm;           // The "Brain" (Handles DB)
    static EventTicker ticker;      // The "Ear" (Handles Live Logs)
    
    static bool is_connected = false;
    static std::string status_msg = "Disconnected";
    
    // --- MAIN LOOP ---
    while (app.NewFrame()) {
        
        // ==================================================================================
        // 1. SIDEBAR (Connection & Settings)
        // ==================================================================================
        ImGui::Begin("Flux Control", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowSize(ImVec2(250, 900));
        ImGui::SetWindowPos(ImVec2(0, 0));

        ImGui::TextDisabled("SERVER CONFIG");
        ImGui::InputText("IP", server_ip, 128);
        ImGui::InputInt("Port", &server_port);
        ImGui::InputText("Pass", password, 128, ImGuiInputTextFlags_Password);
        ImGui::Dummy(ImVec2(0, 10));

        if (ImGui::Button(is_connected ? "Reconnect" : "Connect", ImVec2(230, 40))) {
            // Use Controller to connect
            if (crm.connect(server_ip, server_port, password)) {
                is_connected = true;
                status_msg = "Online";
                // Start the Live Ticker Listener
                ticker.start(server_ip, server_port, password);
            } else {
                is_connected = false;
                status_msg = crm.getError();
            }
        }
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::TextColored(is_connected ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "STATUS: %s", status_msg.c_str());
        ImGui::End();

        // Only show workspace if connected
        if (is_connected) {
            
            // ==================================================================================
            // 2. KANBAN BOARD (Top Section)
            // ==================================================================================
            ImGui::SetNextWindowPos(ImVec2(250, 0));
            ImGui::SetNextWindowSize(ImVec2(1030, 550));
            ImGui::Begin("Pipeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

            // --- Toolbar ---
            if (ImGui::Button("+ New Opportunity", ImVec2(150, 30))) {
                ImGui::OpenPopup("Add Lead Form");
            }

            // --- Popup Modal ---
            if (ImGui::BeginPopupModal("Add Lead Form", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Create a new sales lead");
                ImGui::Separator();

                ImGui::InputText("Contact Name", new_lead_name, 128);
                ImGui::InputText("Company", new_lead_company, 128);
                ImGui::InputInt("Est. Value ($)", &new_lead_value);
                ImGui::Dummy(ImVec2(0, 10));

                if (ImGui::Button("Save Lead", ImVec2(120, 0))) {
                    Lead l;
                    l.name = new_lead_name;
                    l.company = new_lead_company;
                    l.value = new_lead_value;
                    
                    // Use Controller logic
                    if (crm.addLead(l)) {
                        crm.publishEvent("New Lead: " + std::string(new_lead_name));
                        new_lead_name[0] = '\0'; new_lead_company[0] = '\0'; // Reset
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::Separator();

            // --- Columns ---
            const char* stages[] = { "New", "Contacted", "Won" };
            ImGui::Columns(3, "kanban_cols");
            
            // Data Aggregation for Graphs
            double stage_counts[3] = {0,0,0}; 
            double stage_values[3] = {0,0,0};

            for (int i = 0; i < 3; i++) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", stages[i]);
                ImGui::Separator();

                // Fetch Clean Objects from Controller (No raw JSON!)
                std::vector<Lead> leads = crm.getLeadsByStage(stages[i]);
                
                // Store stats for the graph below
                stage_counts[i] = (double)leads.size();

                for (const auto& lead : leads) {
                    stage_values[i] += lead.value;

                    ImGui::PushID(lead.id);
                    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(i * 0.35f, 0.6f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(i * 0.35f, 0.7f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(i * 0.35f, 0.8f, 0.8f));
                    
                    std::string label = lead.name + "\n" + lead.company + "\n$" + std::to_string(lead.value);
                    ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 80));
                    
                    if (ImGui::BeginPopupContextItem()) {
                        ImGui::TextDisabled("Lead #%d", lead.id);
                        ImGui::Separator();

                        if (ImGui::Selectable("View Details")) {
                            selected_lead = lead; // Copy data
                            show_details = true;  // Trigger Modal
                        }

                        // Promote Logic
                        if (i < 2) { 
                            std::string nextStage = stages[i+1];
                            std::string label = "Promote to " + nextStage;
                            
                            if (ImGui::Selectable(label.c_str())) {
                                // Update via Controller
                                if(crm.updateLeadStatus(lead, nextStage)){
                                    crm.publishEvent("Promoted " + lead.name + " to " + nextStage);
                                }
                            }
                        }

                        // Delete Logic
                        if (ImGui::Selectable("Delete")) {
                            crm.deleteLead(lead.id);
                        }
                        
                        ImGui::EndPopup();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                }
                ImGui::NextColumn();
            }
            ImGui::End();

            // --- DETAILS MODAL ---
        if (show_details) {
            ImGui::OpenPopup("Lead Details");
        }

        if (ImGui::BeginPopupModal("Lead Details", &show_details, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", selected_lead.name.c_str());
            ImGui::TextDisabled("%s", selected_lead.company.c_str());
            ImGui::Separator();

            if (ImGui::BeginTabBar("DetailsTabs")) {
                
                // TAB 1: TASKS
                if (ImGui::BeginTabItem("Tasks")) {
                    ImGui::Dummy(ImVec2(0,5));
                    
                    // Add Task Form
                    ImGui::InputText("New Task", new_task_desc, 128);
                    ImGui::SameLine();
                    if (ImGui::Button("Add")) {
                        if (crm.addTask(selected_lead.id, new_task_desc)) {
                            new_task_desc[0] = '\0';
                        }
                    }
                    ImGui::Separator();

                    // Task List
                    auto tasks = crm.getTasks(selected_lead.id);
                    for (auto& task : tasks) {
                        bool done = task.is_done;
                        if (ImGui::Checkbox(("##t" + std::to_string(task.id)).c_str(), &done)) {
                            crm.toggleTask(task.id, done);
                        }
                        ImGui::SameLine();
                        if (done) ImGui::TextDisabled("%s", task.description.c_str());
                        else      ImGui::Text("%s", task.description.c_str());
                    }
                    if (tasks.empty()) ImGui::TextDisabled("No tasks yet.");
                    
                    ImGui::EndTabItem();
                }

                // TAB 2: INTERACTIONS (History)
                if (ImGui::BeginTabItem("History")) {
                    ImGui::Dummy(ImVec2(0,5));
                    
                    // Add Note Form
                    ImGui::InputText("Log Note", new_note_text, 256);
                    ImGui::SameLine();
                    if (ImGui::Button("Log")) {
                        if (crm.addInteraction(selected_lead.id, new_note_text)) {
                            new_note_text[0] = '\0';
                        }
                    }
                    ImGui::Separator();

                    // History List
                    auto history = crm.getInteractions(selected_lead.id);
                    // Show newest first? (Vector push_back is oldest first, reverse loop)
                    for (int k = history.size() - 1; k >= 0; k--) {
                        ImGui::BulletText("%s", history[k].note.c_str());
                    }
                    if (history.empty()) ImGui::TextDisabled("No interactions logged.");

                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Dummy(ImVec2(0, 10));
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                show_details = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

            // ==================================================================================
            // 3. ANALYTICS DASHBOARD (Bottom Section)
            // ==================================================================================
            // Use ImGuiCond_FirstUseEver so it doesn't snap back when you move/resize it!
            ImGui::SetNextWindowPos(ImVec2(250, 500), ImGuiCond_FirstUseEver); 
            ImGui::SetNextWindowSize(ImVec2(1030, 450), ImGuiCond_FirstUseEver); // Increased Height
            ImGui::Begin("Analytics", nullptr, ImGuiWindowFlags_None);

            if (ImPlot::BeginPlot("Pipeline Health", ImVec2(-1, -1))) {
                ImPlot::SetupAxes("Stage", "Total Value ($)");
                ImPlot::SetupAxisTicks(ImAxis_X1, 0, 2, 3, stages);
                
                ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); 
                ImPlot::PlotBars("Revenue Forecast", stage_values, 3);
                
                ImPlot::SetNextLineStyle(ImVec4(1,1,0,1), 3.0f);
                ImPlot::PlotLine("Lead Count", stage_counts, 3);

                ImPlot::EndPlot();
            }
            ImGui::End();

            // ==================================================================================
            // 4. LIVE TICKER OVERLAY
            // ==================================================================================
            float winHeight = 40.0f;
            // Always position at the bottom of the app window
            ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - winHeight));
            ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, winHeight));
            
            ImGui::Begin("Ticker", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
            std::vector<std::string> logs = ticker.getLogs();
            if (!logs.empty()) {
                ImGui::TextColored(ImVec4(0,1,0,1), "LIVE: %s", logs.back().c_str());
            } else {
                ImGui::TextDisabled("Waiting for events...");
            }
            ImGui::End();
        }

        app.Render();
    }

    // Cleanup ImPlot
    ImPlot::DestroyContext();
    return 0;
}