#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>

// GUI 
#include "gui_host.hpp"        
#include "ui/context.hpp"      
#include "ui/sidebar.hpp"      
#include "ui/pipeline.hpp"
#include "ui/analytics.hpp"
#include "ui/statusbar.hpp"

// CLI 
#include "cli/cli_host.hpp"

int main(int argc, char** argv) {

    bool cli_mode = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--cli" || arg == "-c") {
            cli_mode = true;
        }
    }

    if (cli_mode) {
        // --- CLI MODE ---
        CLI::CLIHost host;
        host.run();
    } 
    else {
        // --- GUI MODE ---
        CRM::AppHost app(L"FluxCRM", 1280, 900);
        ImPlot::CreateContext();
        UI::AppState state;

        while (app.NewFrame()) {
            UI::RenderSidebar(state);
            if (state.is_connected) {
                UI::RenderPipeline(state);   
                UI::RenderAnalytics(state);  
                UI::RenderStatusBar(state);  
            }
            if (state.reset_layout) state.reset_layout = false;
            app.Render();
        }
        ImPlot::DestroyContext();
    }

    return 0;
}