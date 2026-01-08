#pragma once
#include "imgui.h"
#include "context.hpp"

namespace UI {
    inline void RenderStatusBar(AppState& state) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        ImVec2 pos = viewport->WorkPos;
        pos.y += viewport->WorkSize.y - UI::STATUSBAR_HEIGHT;
        
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, UI::STATUSBAR_HEIGHT), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                 ImGuiWindowFlags_NoMove | 
                                 ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoSavedSettings | 
                                 ImGuiWindowFlags_NoFocusOnAppearing | 
                                 ImGuiWindowFlags_NoNav;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        
        if (ImGui::Begin("Ticker", nullptr, flags)) {
            
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x, pos.y), 
                ImVec2(pos.x + viewport->WorkSize.x, pos.y), 
                IM_COL32(100, 100, 100, 255), 1.0f
            );

            std::vector<std::string> logs = state.ticker.getLogs();
            
            if (!logs.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text(">> %s", logs.back().c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text("System Online. Listening for events...");
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }
}