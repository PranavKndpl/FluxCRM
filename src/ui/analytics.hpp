#pragma once
#include "imgui.h"
#include "implot.h"
#include "context.hpp"

namespace UI {
    inline void RenderAnalytics(AppState& state) {
        ImGuiCond cond = state.reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

        ImGui::SetNextWindowPos(ImVec2(UI::SIDEBAR_WIDTH, UI::PIPELINE_HEIGHT), cond); 
        
        
        float width = ImGui::GetIO().DisplaySize.x - UI::SIDEBAR_WIDTH;
        float height = ImGui::GetIO().DisplaySize.y - UI::PIPELINE_HEIGHT - UI::STATUSBAR_HEIGHT;
        
        ImGui::SetNextWindowSize(ImVec2(width, height), cond);
        
        ImGui::Begin("Analytics", nullptr, ImGuiWindowFlags_None);
        if (ImPlot::BeginPlot("Pipeline Health", ImVec2(-1, -1))) {
            ImPlot::SetupAxes("Stage", "Total Value ($)");
            ImPlot::SetupAxisTicks(ImAxis_X1, 0, 2, 3, state.stages);
            
            ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); 
            ImPlot::PlotBars("Revenue Forecast", state.stage_values, 3);
            
            ImPlot::SetNextLineStyle(ImVec4(1,1,0,1), 3.0f);
            ImPlot::PlotLine("Lead Count", state.stage_counts, 3);

            ImPlot::EndPlot();
        }
        ImGui::End();
    }
}