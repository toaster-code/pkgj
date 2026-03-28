#include "logviewer.hpp"

#include "imgui.hpp"
#include "logbuffer.hpp"

extern "C"
{
#include "style.h"
}

namespace
{
constexpr float ViewerW = VITA_WIDTH * 0.9f;
constexpr float ViewerH = VITA_HEIGHT * 0.85f;
}

void LogViewer::render(const pkgi_input& input)
{
    const auto lines = pkgi_log_buffer_snapshot();
    const int  n     = static_cast<int>(lines.size());

    // ── Navigation ─────────────────────────────────────────────────────────────────
    // input.pressed: single press (read before pkgi.cpp zeroes it)
    // input.down:    held every frame
    const int dir = (input.down & PKGI_BUTTON_UP)   ? -1
                  : (input.down & PKGI_BUTTON_DOWN)  ? +1
                  : 0;

    bool nav_step = false;

    // Initial press fires immediately
    if ((input.pressed & PKGI_BUTTON_UP) && n > 0)
    {
        _selected = std::max(0, _selected - 1);
        nav_step  = true;
        _scroll_counter = 0;
    }
    else if ((input.pressed & PKGI_BUTTON_DOWN) && n > 0)
    {
        _selected = std::min(n - 1, _selected + 1);
        nav_step  = true;
        _scroll_counter = 0;
    }
    else if (dir == 0)
    {
        _scroll_counter = 0;
    }
    else
    {
        // Repeat while held: fire every ScrollThreshold frames
        if (++_scroll_counter >= ScrollThreshold)
        {
            _scroll_counter = 0;
            if (n > 0)
                _selected = std::max(0, std::min(_selected + dir, n - 1));
            nav_step = true;
        }
    }

    // Clamp in case log buffer shrank
    if (n == 0)
        _selected = 0;
    else
        _selected = std::max(0, std::min(_selected, n - 1));

    // ── Window ────────────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH  - ViewerW) / 2.f,
                   (VITA_HEIGHT - ViewerH) / 2.f));
    ImGui::SetNextWindowSize(ImVec2(ViewerW, ViewerH), 0);

    ImGui::Begin(
            "Log Viewer###logviewer",
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::Text("Lines: %d", n);
    ImGui::Separator();

    const float footer_h =
            ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

    // NoNav: we handle navigation ourselves
    ImGui::BeginChild(
            "##logrows",
            ImVec2(0.f, -footer_h),
            false,
            ImGuiWindowFlags_NoNav);

    if (n == 0)
    {
        ImGui::TextDisabled("(no log messages yet)");
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            ImGui::PushID(i);

            const bool selected = (i == _selected);
            const char* text    = lines[i].empty() ? " " : lines[i].c_str();

            // Highlight selected row without using ImGui navigation
            ImGui::Selectable(text, selected, ImGuiSelectableFlags_Disabled);

            // Scroll the selected row into view whenever navigation occurred
            if (selected && nav_step)
                ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled(
            "[Up/Down] navigate   "
            "(hold 1s = fast scroll)   "
            "[Circle] close");

    ImGui::End();
}