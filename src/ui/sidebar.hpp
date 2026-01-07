#pragma once
#include "imgui.h" // User requested shortened paths
#include "context.hpp"

namespace UI {
    
    inline void RenderSidebar(AppState& state) {
        ImGui::Begin("Flux Control", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowSize(ImVec2(250, 900));
        ImGui::SetWindowPos(ImVec2(0, 0));

        ImGui::TextDisabled("SERVER CONFIG");
        ImGui::InputText("IP", state.server_ip, 128);
        ImGui::InputInt("Port", &state.server_port);
        ImGui::InputText("Pass", state.password, 128, ImGuiInputTextFlags_Password);
        
        ImGui::Dummy(ImVec2(0, 10));
        
        if (ImGui::Button(state.is_connected ? "Reconnect" : "Connect", ImVec2(230, 40))) {
            if (state.crm.connect(state.server_ip, state.server_port, state.password)) {
                state.is_connected = true;
                state.status_msg = "Online";
                state.ticker.start(state.server_ip, state.server_port, state.password);
            } else {
                state.is_connected = false;
                state.status_msg = state.crm.getError();
            }
        }
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::TextColored(state.is_connected ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "STATUS: %s", state.status_msg.c_str());

        // --- LAYOUT CONTROLS ---
        ImGui::Dummy(ImVec2(0, 30)); // Spacer
        ImGui::Separator();
        ImGui::TextDisabled("WINDOW CONTROLS");
        if (ImGui::Button("Reset Layout", ImVec2(230, 30))) {
            state.reset_layout = true; // <--- Trigger the reset
        }
        
        ImGui::End();
    }
}