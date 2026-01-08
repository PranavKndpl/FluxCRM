#pragma once
#include "imgui.h"
#include "imgui_internal.h" 
#include "context.hpp"
#include <string>
#include <vector>
#include <cctype>

namespace UI {

    // --- HELPERS ---

    static bool IsValidDate(const std::string& date) {
        if (date.length() != 10) return false;
        if (date[4] != '-' || date[7] != '-') return false;
        for (int i = 0; i < 10; i++) {
            if (i == 4 || i == 7) continue;
            if (!isdigit(date[i])) return false;
        }
        int m = std::stoi(date.substr(5, 2));
        int d = std::stoi(date.substr(8, 2));
        return (m >= 1 && m <= 12) && (d >= 1 && d <= 31);
    }

    static char search_query[128] = "";

    // --- COMPONENT: ADD LEAD MODAL ---
    static void RenderAddLeadModal(AppState& state) {
        if (ImGui::BeginPopupModal("Add Lead Form", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char name[128] = ""; static char company[128] = ""; static int value = 1000;
            
            ImGui::Text("Create a new sales lead"); ImGui::Separator();
            ImGui::InputText("Contact Name", name, 128);
            ImGui::InputText("Company", company, 128);
            ImGui::InputInt("Est. Value ($)", &value);
            ImGui::Dummy(ImVec2(0, 10));

            bool isValid = (strlen(name) > 0 && strlen(company) > 0);
            if (!isValid) ImGui::BeginDisabled();
            
            if (ImGui::Button("Save Lead", ImVec2(120, 0))) {
                Lead l; l.name = name; l.company = company; l.value = value;
                if (state.crm.addLead(l)) {
                    state.crm.publishEvent("New Lead: " + std::string(name));
                    name[0] = '\0'; company[0] = '\0'; 
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!isValid) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    // --- COMPONENT: DETAILS MODAL ---
    static void RenderDetailsModal(AppState& state) {
        if (state.show_details_modal) ImGui::OpenPopup("Lead Details");

        if (ImGui::BeginPopupModal("Lead Details", &state.show_details_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", state.selected_lead.name.c_str());
            ImGui::TextDisabled("%s", state.selected_lead.company.c_str());
            ImGui::Separator();

            if (ImGui::BeginTabBar("DetailsTabs")) {
                // TASKS TAB
                if (ImGui::BeginTabItem("Tasks")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    static char new_task[128] = ""; static char due_date[20] = "";
                    ImGui::InputText("Description", new_task, 128);
                    
                    bool is_date_valid = IsValidDate(due_date);
                    ImGui::InputTextWithHint("Due Date", "YYYY-MM-DD", due_date, 20);
                    
                    if (!is_date_valid && due_date[0] != '\0') 
                        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Format: YYYY-MM-DD");

                    bool valid = strlen(new_task) > 0 && (is_date_valid || due_date[0] == '\0');
                    if (!valid) ImGui::BeginDisabled();
                    
                    if (ImGui::Button("Add Task")) {
                        if (state.crm.addTask(state.selected_lead.id, new_task, due_date)) {
                            state.crm.publishEvent("New Task: " + std::string(new_task));
                            new_task[0] = '\0'; due_date[0] = '\0';
                        }
                    }
                    if (!valid) ImGui::EndDisabled();

                    ImGui::Separator();
                    auto tasks = state.crm.getTasks(state.selected_lead.id);
                    for (auto& t : tasks) {
                        bool done = t.is_done;
                        if (ImGui::Checkbox(("##t" + std::to_string(t.id)).c_str(), &done))
                            state.crm.toggleTask(t.id, done);
                        ImGui::SameLine();
                        ImGui::TextDisabled(done ? "%s" : "%s", t.description.c_str());
                        if(!t.due_date.empty()) {
                             ImGui::SameLine(); 
                             ImGui::TextColored(ImVec4(1,0.5f,0,1), "(%s)", t.due_date.c_str());
                        }
                    }
                    ImGui::EndTabItem();
                }

                // HISTORY TAB
                if (ImGui::BeginTabItem("History")) {
                    static char note[256] = "";
                    ImGui::InputText("Note", note, 256);
                    ImGui::SameLine();

                    bool has_text = strlen(note) > 0;
                    if (!has_text) ImGui::BeginDisabled();

                    if (ImGui::Button("Log")) {
                        if (state.crm.addInteraction(state.selected_lead.id, note)) {
                             state.crm.publishEvent("Note: " + std::string(note)); 
                             note[0] = '\0';
                        }
                    }
                    
                    if (!has_text) ImGui::EndDisabled();
                    if(ImGui::Button("Clear")) state.crm.clearInteractions(state.selected_lead.id);

                    ImGui::Separator();
                    auto history = state.crm.getInteractions(state.selected_lead.id);
                    for (auto& h : history) ImGui::BulletText("%s", h.note.c_str());
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Dummy(ImVec2(0, 10));
            if (ImGui::Button("Close")) { state.show_details_modal = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }

    // --- MAIN PIPELINE RENDERER ---
    inline void RenderPipeline(AppState& state) {
        
        ImGuiCond cond = state.reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        float width = ImGui::GetIO().DisplaySize.x - UI::SIDEBAR_WIDTH;
        ImGui::SetNextWindowPos(ImVec2(UI::SIDEBAR_WIDTH, 0), cond);
        ImGui::SetNextWindowSize(ImVec2(width, UI::PIPELINE_HEIGHT), cond);

        ImGui::Begin("Pipeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);


        if (ImGui::Button("+ New Opportunity", ImVec2(150, 30))) 
            ImGui::OpenPopup("Add Lead Form");

        ImGui::SameLine();
        float search_w = 250.0f;
        float cursor_x = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - search_w;
        if (cursor_x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(cursor_x); 
        
        ImGui::SetNextItemWidth(search_w);
        ImGui::InputTextWithHint("##search", "Search Leads...", search_query, 128);

        RenderAddLeadModal(state);
        ImGui::Separator();

        // TABLE RENDERING
        if (ImGui::BeginTable("pipeline", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {

            ImGui::TableSetupColumn("New", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Contacted", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Won", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();

            for (int i = 0; i < 3; i++) {
                ImGui::TableSetColumnIndex(i);

                // --- DROP TARGET ---
                ImVec2 min = ImGui::GetCursorScreenPos();
                ImVec2 max = ImVec2(min.x + ImGui::GetColumnWidth(), min.y + ImGui::GetContentRegionAvail().y);
                ImRect rect(min, max);

                if (ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID(i))) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LEAD_MOVE")) {
                        // Using sizeof(fluxdb::Id) to be safe with 64-bit IDs
                        fluxdb::Id id = *(const fluxdb::Id*)payload->Data; 
                        state.crm.moveLead(id, state.stages[i]);
                        state.crm.publishEvent("Moved lead to " + std::string(state.stages[i]));
                    }
                    ImGui::EndDragDropTarget();
                }

                auto leads = state.crm.getLeadsByStage(state.stages[i]);
                state.stage_counts[i] = (double)leads.size();

                for (auto& lead : leads) {
                    state.stage_values[i] += lead.value;

                    if (search_query[0] && 
                        lead.name.find(search_query) == std::string::npos && 
                        lead.company.find(search_query) == std::string::npos) 
                        continue;

                    ImGui::PushID((int)lead.id);
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(i * 0.35f, 0.6f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(i * 0.35f, 0.7f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(i * 0.35f, 0.8f, 0.8f));

                    std::string label = lead.name + "\n" + lead.company + "\n$" + std::to_string(lead.value);
                    ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 70));

                    // DRAG SOURCE
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("LEAD_MOVE", &lead.id, sizeof(fluxdb::Id));
                        ImGui::Text("Move %s", lead.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    // CONTEXT MENU
                    if (ImGui::BeginPopupContextItem()) {
                         if (ImGui::Selectable("Details")) { state.selected_lead = lead; state.show_details_modal = true; }
                         if (ImGui::Selectable("Delete")) state.crm.deleteLead(lead.id);
                         ImGui::EndPopup();
                    }

                    ImGui::PopStyleColor(3); 
                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0, 5));
                }
            }
            ImGui::EndTable();
        }

        ImGui::End();
        RenderDetailsModal(state);
    }
}