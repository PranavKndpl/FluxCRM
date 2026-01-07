#pragma once
#include "../crm_core.hpp" 
#include <vector>
#include <string>

namespace UI {

    struct AppState {
        // Core Systems
        CRMSystem crm;
        EventTicker ticker;
        
        // Connection State
        char server_ip[128] = "127.0.0.1";
        int server_port = 8080;
        char password[128] = "flux_admin";
        bool is_connected = false;
        std::string status_msg = "Disconnected";

        // Layout State
        bool reset_layout = false; // <--- NEW FLAG

        // Shared Data
        double stage_counts[3] = {0, 0, 0};
        double stage_values[3] = {0, 0, 0};
        const char* stages[3] = { "New", "Contacted", "Won" };

        // Modal/Selection State
        bool show_details_modal = false;
        Lead selected_lead; 
    };
}