#pragma once
#include "../crm_core.hpp"
#include "imgui.h"
#include <string>

namespace UI {

class Kanban {
private:
    // Local State 
    char new_lead_name[128] = "";
    char new_lead_company[128] = "";
    int new_lead_value = 1000;
    bool show_error = false;

public:
    void Render(CRMSystem& crm, bool is_connected) {
        if (!is_connected) return;

        ImGui::SetNextWindowPos(ImVec2(250, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1030, 550), ImGuiCond_Always);
        
        ImGui::Begin("Pipeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        // --- Toolbar ---
        if (ImGui::Button("+ New Opportunity", ImVec2(150, 30))) {
            ImGui::OpenPopup("Add Lead Form");
            // Reset state on open
            new_lead_name[0] = '\0';
            new_lead_company[0] = '\0';
            show_error = false;
        }

        // --- Popup Modal ---
        if (ImGui::BeginPopupModal("Add Lead Form", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Create a new sales lead");
            ImGui::Separator();

            ImGui::InputText("Contact Name", new_lead_name, 128);
            ImGui::InputText("Company", new_lead_company, 128);
            ImGui::InputInt("Est. Value ($)", &new_lead_value);

            if (show_error) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Name and Company are required!");
            }

            ImGui::Dummy(ImVec2(0, 10));

            // string validation
            if (ImGui::Button("Save Lead", ImVec2(120, 0))) {
                if (strlen(new_lead_name) == 0 || strlen(new_lead_company) == 0) {
                    show_error = true;
                } else {
                    Lead l;
                    l.name = new_lead_name;
                    l.company = new_lead_company;
                    l.value = new_lead_value;

                    if (crm.addLead(l)) {
                        crm.publishEvent("New Lead: " + std::string(new_lead_name));
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Separator();
        const char* stages[] = { "New", "Contacted", "Won" };
        ImGui::Columns(3, "kanban_cols");
        
        for (int i = 0; i < 3; i++) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", stages[i]);
            ImGui::Separator();
            
            auto leads = crm.getLeadsByStage(stages[i]);
            for (const auto& lead : leads) {
                ImGui::PushID(lead.id);
                std::string label = lead.name + "\n" + lead.company + "\n$" + std::to_string(lead.value);
                ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 80));
                
                ImGui::PopID();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
            ImGui::NextColumn();
        }
        ImGui::End();
    }
};
}