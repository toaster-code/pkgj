#include "logviewer.hpp"

#include "imgui.hpp"
#include "logbuffer.hpp"

extern "C"
{
#include "style.h"
}

#include <algorithm>

namespace
{
constexpr float ViewerW = VITA_WIDTH * 0.9f;
constexpr float ViewerH = VITA_HEIGHT * 0.85f;
}

void LogViewer::render()
{
    const auto lines = pkgi_log_buffer_snapshot();

    if (lines.empty())
        _selected = 0;
    else
        _selected = std::max(0, std::min(_selected, static_cast<int>(lines.size()) - 1));

    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH - ViewerW) / 2.f,
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

    ImGui::Text("Entries: %d", static_cast<int>(lines.size()));
    ImGui::Separator();

    const float footer_h =
            ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild(
            "##logrows",
            ImVec2(0.f, -footer_h),
            false,
            ImGuiWindowFlags_NavFlattened);

    if (lines.empty())
    {
        ImGui::TextDisabled("(no log messages yet)");
    }
    else
    {
        for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        {
            ImGui::PushID(i);

            const bool selected = (i == _selected);
            const bool activated = ImGui::Selectable(
                    lines[i].empty() ? " " : lines[i].c_str(),
                    selected,
                    ImGuiSelectableFlags_AllowDoubleClick);

            if (selected)
                ImGui::SetItemDefaultFocus();

            if (ImGui::IsItemFocused() || activated)
                _selected = i;

            if (selected && !ImGui::IsItemVisible())
                ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("[Circle] Close");

    ImGui::End();
}