#pragma once
#include "imgui.h"
#include "context.hpp"

namespace UI {

    // --- INTERNAL HELPERS ---
    static void RenderAddLeadModal(AppState& state) {
        if (ImGui::BeginPopupModal("Add Lead Form", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char name[128] = "";
            static char company[128] = "";
            static int value = 1000;
            static char error_msg[64] = ""; // To show validation errors

            ImGui::Text("Create a new sales lead");
            ImGui::Separator();

            ImGui::InputText("Contact Name", name, 128);
            ImGui::InputText("Company", company, 128);
            ImGui::InputInt("Est. Value ($)", &value);

            ImGui::Dummy(ImVec2(0, 10));

            // --- VALIDATION LOGIC ---
            bool isValid = (strlen(name) > 0 && strlen(company) > 0);

            if (isValid) {
                if (ImGui::Button("Save Lead", ImVec2(120, 0))) {
                    Lead l;
                    l.name = name;
                    l.company = company;
                    l.value = value;

                    if (state.crm.addLead(l)) {
                        state.crm.publishEvent("New Lead: " + std::string(name));
                        name[0] = '\0';
                        company[0] = '\0';
                        error_msg[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                }
            } else {
                // Disabled Button Style
                ImGui::BeginDisabled();
                ImGui::Button("Save Lead", ImVec2(120, 0));
                ImGui::EndDisabled();

                // Show why it's disabled
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "* Required");
            }

            // Cancel Button (always active)
            if (!isValid) ImGui::SameLine(); // Alignment fix

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                error_msg[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    static void RenderDetailsModal(AppState& state) {
        if (state.show_details_modal) {
            ImGui::OpenPopup("Lead Details");
        }

        if (ImGui::BeginPopupModal(
                "Lead Details",
                &state.show_details_modal,
                ImGuiWindowFlags_AlwaysAutoResize)) {

            static char new_task[128] = "";
            static char new_note[256] = "";

            ImGui::TextColored(
                ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "%s",
                state.selected_lead.name.c_str());

            ImGui::TextDisabled("%s", state.selected_lead.company.c_str());
            ImGui::Separator();

            if (ImGui::BeginTabBar("DetailsTabs")) {

                // TAB 1: TASKS
                if (ImGui::BeginTabItem("Tasks")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    ImGui::InputText("New Task", new_task, 128);
                    ImGui::SameLine();

                    if (ImGui::Button("Add")) {
                        if (strlen(new_task) > 0) { // Basic check
                            if (state.crm.addTask(state.selected_lead.id, new_task)) {
                                // Publish Event
                                state.crm.publishEvent("New Task: " + std::string(new_task));
                                new_task[0] = '\0';
                            }
                        }
                    }

                    ImGui::Separator();

                    auto tasks = state.crm.getTasks(state.selected_lead.id);
                    for (auto& task : tasks) {
                        bool done = task.is_done;
                        if (ImGui::Checkbox(("##t" + std::to_string(task.id)).c_str(), &done)) {
                            state.crm.toggleTask(task.id, done);
                            // Publish Event
                            std::string status = done ? "Completed: " : "Re-opened: ";
                            state.crm.publishEvent(status + task.description);
                        }

                        ImGui::SameLine();
                        if (done)
                            ImGui::TextDisabled("%s", task.description.c_str());
                        else
                            ImGui::Text("%s", task.description.c_str());
                    }

                    if (tasks.empty())
                        ImGui::TextDisabled("No tasks yet.");

                    ImGui::EndTabItem();
                }

                // TAB 2: HISTORY
                if (ImGui::BeginTabItem("History")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    ImGui::InputText("Log Note", new_note, 256);
                    ImGui::SameLine();

                    if (ImGui::Button("Log")) {
                        if (state.crm.addInteraction(state.selected_lead.id, new_note)) {
                             // Publish note event
                             state.crm.publishEvent("Note Added: " + std::string(new_note));
                             new_note[0] = '\0';
                        }
                    }

                    ImGui::Separator();

                    auto history =
                        state.crm.getInteractions(state.selected_lead.id);
                    for (int k = (int)history.size() - 1; k >= 0; k--) {
                        ImGui::BulletText(
                            "%s",
                            history[k].note.c_str());
                    }

                    if (history.empty())
                        ImGui::TextDisabled("No interactions logged.");

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Dummy(ImVec2(0, 10));
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                state.show_details_modal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // --- MAIN PIPELINE RENDER ---
    inline void RenderPipeline(AppState& state) {

        // --- LAYOUT RESET LOGIC ---
        // If 'reset_layout' is true, we force Always. Otherwise, we respect user movement.
        ImGuiCond cond =
            state.reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

        ImGui::SetNextWindowPos(ImVec2(250, 0), cond);
        ImGui::SetNextWindowSize(ImVec2(1030, 550), cond);

        ImGui::Begin(
            "Pipeline",
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        // Toolbar
        if (ImGui::Button("+ New Opportunity", ImVec2(150, 30))) {
            ImGui::OpenPopup("Add Lead Form");
        }

        RenderAddLeadModal(state);
        ImGui::Separator();

        // Columns setup
        ImGui::Columns(3, "kanban_cols");

        // Reset stats for this frame
        for (int i = 0; i < 3; i++) {
            state.stage_counts[i] = 0;
            state.stage_values[i] = 0;
        }

        for (int i = 0; i < 3; i++) {
            ImGui::TextColored(
                ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                "%s",
                state.stages[i]);

            ImGui::Separator();

            std::vector<Lead> leads =
                state.crm.getLeadsByStage(state.stages[i]);

            state.stage_counts[i] = (double)leads.size();

            for (const auto& lead : leads) {
                state.stage_values[i] += lead.value;

                ImGui::PushID(lead.id);
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    (ImVec4)ImColor::HSV(i * 0.35f, 0.6f, 0.6f));
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    (ImVec4)ImColor::HSV(i * 0.35f, 0.7f, 0.7f));
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    (ImVec4)ImColor::HSV(i * 0.35f, 0.8f, 0.8f));

                std::string label =
                    lead.name + "\n" +
                    lead.company + "\n$" +
                    std::to_string(lead.value);

                ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 80));

                // Context Menu
                if (ImGui::BeginPopupContextItem()) {
                    ImGui::TextDisabled("Lead #%llu", lead.id);
                    ImGui::Separator();

                    if (ImGui::Selectable("View Details")) {
                        state.selected_lead = lead;
                        state.show_details_modal = true;
                    }

                    if (i < 2) {
                        std::string nextStage = state.stages[i + 1];
                        std::string pLabel =
                            "Promote to " + nextStage;

                        if (ImGui::Selectable(pLabel.c_str())) {
                            if (state.crm.updateLeadStatus(
                                    lead,
                                    nextStage)) {
                                state.crm.publishEvent(
                                    "Promoted " +
                                    lead.name +
                                    " to " +
                                    nextStage);
                            }
                        }
                    }

                    if (ImGui::Selectable("Delete")) {
                        state.crm.deleteLead(lead.id);
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

        // Handle Details Modal
        RenderDetailsModal(state);
    }
}
