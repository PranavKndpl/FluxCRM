#pragma once
#include "imgui.h"
#include "context.hpp"

namespace UI {
    
    inline void RenderSidebar(AppState& state) {
        ImGuiIO& io = ImGui::GetIO();
        float height = io.DisplaySize.y - UI::STATUSBAR_HEIGHT;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(UI::SIDEBAR_WIDTH, height), ImGuiCond_Always);

        ImGui::Begin("Flux Control", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // CONNECTION
        ImGui::TextDisabled("CONNECTION");
        ImGui::InputText("IP", state.server_ip, 128);
        ImGui::InputInt("Port", &state.server_port);
        ImGui::InputText("Pass", state.password, 128, ImGuiInputTextFlags_Password);
        ImGui::Dummy(ImVec2(0, 5));
        
        if (state.is_connected) {
             if(ImGui::Button("Disconnect", ImVec2(-1, 0))) { state.is_connected = false; state.status_msg = "Disconnected"; }
        } else {
             if(ImGui::Button("Connect", ImVec2(-1, 0))) { 
                 if (state.crm.connect(state.server_ip, state.server_port, state.password)) {
                    state.is_connected = true; state.status_msg = "Online"; state.ticker.start(state.server_ip, state.server_port, state.password);
                 } else { state.status_msg = state.crm.getError(); }
             }
        }
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::TextColored(state.is_connected ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "STATUS: %s", state.status_msg.c_str());
        ImGui::Separator();

        if (state.is_connected) {
            ImGui::Dummy(ImVec2(0, 10));
            ImGui::TextDisabled("PERFORMANCE");
            
            double currentRevenue = state.crm.getWonRevenue();
            double goal = state.crm.getPerformanceGoal(); 
            float progress = (goal > 0) ? (float)(currentRevenue / goal) : 0.0f;
            if (progress > 1.0f) progress = 1.0f;

            char overlay[64];
            sprintf(overlay, "$%.0f / $%.0f", currentRevenue, goal);

            if (progress >= 1.0f) ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0,1,0,1));
            ImGui::ProgressBar(progress, ImVec2(-1, 0), overlay);
            if (progress >= 1.0f) ImGui::PopStyleColor();

            // 3. ALERTS
            ImGui::Dummy(ImVec2(0, 20));
            ImGui::Separator();
            ImGui::TextDisabled("ALERTS");
            
            // Fetch Overdue Tasks
            std::vector<Task> overdue = state.crm.getOverdueTasks();

            if (!overdue.empty()) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.1f, 0.1f, 0.5f)); 
                ImGui::BeginChild("Alerts", ImVec2(0, 150), true);
                
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "[!] %d OVERDUE TASKS", (int)overdue.size());
                ImGui::Separator();
                
                for (auto& t : overdue) {
                     ImGui::BulletText("%s", t.description.c_str());
                     ImGui::TextDisabled("   Due: %s", t.due_date.c_str());
                     ImGui::Dummy(ImVec2(0, 3));
                }
                
                ImGui::EndChild();
                ImGui::PopStyleColor();
            } else {
                ImGui::TextColored(ImVec4(0,1,0,1), "All Clear. No overdue tasks.");
            }
        }

        // CONTROLS
        float current_y = ImGui::GetCursorPosY();
        float bottom_y = height - 120; 
        if (current_y < bottom_y) ImGui::SetCursorPosY(bottom_y);

        ImGui::Separator();
        ImGui::TextDisabled("SYSTEM CONTROLS");
        
        if (ImGui::Button("Reset Layout", ImVec2(-1, 30))) state.reset_layout = true;
        
        if (ImGui::Button("Clear System Logs", ImVec2(-1, 30))) {
            state.show_clear_confirm = true; 
        }

        // --- CONFIRMATION MODAL ---
        if (state.show_clear_confirm) {
            ImGui::OpenPopup("Confirm Delete");
        }

        if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete ALL system logs?");
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "This action cannot be undone.");
            ImGui::Dummy(ImVec2(0, 10));

            if (ImGui::Button("Yes, Delete All", ImVec2(120, 0))) {
                // Delete from DB
                int count = state.crm.clearInteractions(-1); // -1 = All
                
                state.ticker.clear();
                
                state.status_msg = "Wiped " + std::to_string(count) + " logs.";
                state.show_clear_confirm = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                state.show_clear_confirm = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::End();
    }
}