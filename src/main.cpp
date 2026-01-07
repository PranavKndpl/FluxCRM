// --- INCLUDES ---
// 1. System Headers (MUST be first to fix winsock warning)
#include <winsock2.h>
#include <windows.h>

// 2. Logic Headers
#include "gui_host.hpp"        
#include "ui/context.hpp"      
#include "ui/sidebar.hpp"      
#include "ui/pipeline.hpp"
#include "ui/analytics.hpp"
#include "ui/statusbar.hpp"

// --- MAIN ENTRY POINT ---
int main(int, char**) {
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
        
        // --- CRITICAL: Clear the reset flag after one frame ---
        if (state.reset_layout) {
            state.reset_layout = false;
        }

        app.Render();
    }

    ImPlot::DestroyContext();
    return 0;
}