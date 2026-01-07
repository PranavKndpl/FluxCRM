#pragma once
#include "imgui.h"
#include "context.hpp"

namespace UI {
    inline void RenderStatusBar(AppState& state) {
        float winHeight = 40.0f;
        // Dock to bottom
        ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - winHeight));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, winHeight));
        
        // NoTitleBar + NoBackground makes it look like a transparent overlay
        ImGui::Begin("Ticker", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
        
        std::vector<std::string> logs = state.ticker.getLogs();
        if (!logs.empty()) {
            // CHANGED: Brighter Cyan (0.2, 1.0, 1.0) for the event text
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 1.0f, 1.0f), ">> %s", logs.back().c_str());
        } else {
            // CHANGED: Brighter Grey (0.7) instead of TextDisabled (which was too dark)
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Waiting for events...");
        }
        ImGui::End();
    }
}