#include "gameview.hpp"

#include <fmt/format.h>

#include "dialog.hpp"
#include "file.hpp"
#include "imgui.hpp"
#include "pkgi.hpp"
extern "C"
{
#include "style.h"
}

namespace
{
constexpr unsigned GameViewWidth  = VITA_WIDTH  * 0.8;
constexpr unsigned GameViewHeight = VITA_HEIGHT * 0.8;

// Thumbnail panel size presets indexed by config.thumbnail_size
// 0=off, 1=small, 2=medium, 3=large
struct ThumbSize { float w, h; };
constexpr ThumbSize kThumbSizes[] = {
    {  0.f,   0.f}, // 0 off
    {130.f, 110.f}, // 1 small
    {200.f, 170.f}, // 2 medium
    {260.f, 221.f}, // 3 large (previous default)
};
constexpr int kThumbSizeCount = 4;
}

GameView::GameView(
        const Config* config,
        Downloader* downloader,
        DbItem* item,
        std::optional<CompPackDatabase::Item> base_comppack,
        std::optional<CompPackDatabase::Item> patch_comppack,
        AnnotationDatabase* annotationDb)
    : _config(config)
    , _downloader(downloader)
    , _item(item)
    , _base_comppack(base_comppack)
    , _patch_comppack(patch_comppack)
    , _patch_info_fetcher(item->titleid)
    , _image_fetcher(config, item)
    , _annotationDb(annotationDb)
    , _annotation(annotationDb ? annotationDb->get(item->titleid) : UserAnnotation{})
{
    // Populate the text buffer from the saved annotation
    std::strncpy(_comment_buf, _annotation.comment.c_str(),
                 sizeof(_comment_buf) - 1);
    _comment_buf[sizeof(_comment_buf) - 1] = '\0';

    refresh();
}

void GameView::render()
{
    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH - GameViewWidth) / 2,
                   (VITA_HEIGHT - GameViewHeight) / 2));
    ImGui::SetNextWindowSize(ImVec2(GameViewWidth, GameViewHeight), 0);

    ImGui::Begin(
            fmt::format("{} ({})###gameview", _item->name, _item->titleid)
                    .c_str(),
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings);

    // ── Thumbnail panel — drawn directly onto the window draw list ──────────
    //
    // Using GetWindowDrawList() anchors the panel at absolute screen
    // coordinates, which gives two important properties:
    //   1. The panel never scrolls off-screen when the window is scrolled
    //      via gamepad navigation — it stays fixed at the top-right corner.
    //   2. Draw list primitives are completely outside ImGui's widget and
    //      navigation system, so the D-pad can never focus or select the image.
    {
        const int tsz = std::max(0, std::min(
                _config->thumbnail_size, kThumbSizeCount - 1));
        const float kImagePanelW = kThumbSizes[tsz].w;
        const float kImagePanelH = kThumbSizes[tsz].h;

        auto* thumb_tex = _image_fetcher.get_texture();

        if (kImagePanelW > 0.f)
        {
        // Anchor panel using only the window's fixed screen position +
        // compile-time constants. This is 100% independent of scroll state,
        // content region, or any internal ImGui window state that can change
        // when gamepad navigation scrolls the content.
        //   x: right edge of the window (pos + fixed width) minus panel width and padding
        //   y: just below the title bar (pos + title bar height + padding)
        const ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 win_pos = ImGui::GetWindowPos();
        const float title_bar_h = ImGui::GetFrameHeight(); // title bar height

        ImVec2 panel_min(
                win_pos.x + (float)GameViewWidth
                        - style.WindowPadding.x - kImagePanelW,
                win_pos.y + title_bar_h + style.WindowPadding.y);
        ImVec2 panel_max(
                panel_min.x + kImagePanelW,
                panel_min.y + kImagePanelH);

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // Background fill and border
        dl->AddRectFilled(panel_min, panel_max, IM_COL32(20, 20, 20, 230), 3.f);
        dl->AddRect(panel_min, panel_max, IM_COL32(110, 110, 110, 255), 3.f);

        if (thumb_tex)
        {
            float tw = static_cast<float>(vita2d_texture_get_width(thumb_tex));
            float th = static_cast<float>(vita2d_texture_get_height(thumb_tex));
            // Scale to fit inside the panel with a small margin, keeping ratio
            const float inner_w = kImagePanelW - 6.f;
            const float inner_h = kImagePanelH - 6.f;
            if (tw > inner_w) { th = th * inner_w / tw; tw = inner_w; }
            if (th > inner_h) { tw = tw * inner_h / th; th = inner_h; }
            // Centre the image inside the panel
            ImVec2 img_min(
                    panel_min.x + (kImagePanelW - tw) * 0.5f,
                    panel_min.y + (kImagePanelH - th) * 0.5f);
            ImVec2 img_max(img_min.x + tw, img_min.y + th);
            dl->AddImage((ImTextureID)thumb_tex, img_min, img_max);
        }
        else
        {
            // Two centred lines when there is no image yet
            const char* line1 = "No image";
            const char* line2 = "available";
            ImVec2 s1 = ImGui::CalcTextSize(line1);
            ImVec2 s2 = ImGui::CalcTextSize(line2);
            const float gap     = 2.f;
            const float total_h = s1.y + gap + s2.y;
            const ImU32 dim     = IM_COL32(160, 160, 160, 200);
            dl->AddText(
                    ImVec2(panel_min.x + (kImagePanelW - s1.x) * 0.5f,
                           panel_min.y + (kImagePanelH - total_h) * 0.5f),
                    dim, line1);
            dl->AddText(
                    ImVec2(panel_min.x + (kImagePanelW - s2.x) * 0.5f,
                           panel_min.y + (kImagePanelH - total_h) * 0.5f
                                   + s1.y + gap),
                    dim, line2);
        }
        } // end if (kImagePanelW > 0.f)
    }
    // ── end thumbnail panel ──────────────────────────────────────────────────

    // Reserve the right column for the image panel; text wraps within the rest.
    // PushTextWrapPos takes a window-local X coordinate.
    // When thumbnail is off (size 0), use full-width wrap (pos = 0).
    {
        const int tsz = std::max(0, std::min(
                _config->thumbnail_size, kThumbSizeCount - 1));
        const float panelW = kThumbSizes[tsz].w;
        ImGui::PushTextWrapPos(
                panelW > 0.f
                ? (float)GameViewWidth
                        - ImGui::GetStyle().WindowPadding.x
                        - panelW
                        - ImGui::GetStyle().ItemSpacing.x
                : 0.f);
    }

    ImGui::Text(fmt::format("Firmware version: {}", pkgi_get_system_version())
                        .c_str());
    ImGui::Text(
            fmt::format(
                    "Required firmware version: {}", get_min_system_version())
                    .c_str());

    ImGui::Text(" ");

    ImGui::Text(fmt::format(
                        "Installed game version: {}",
                        _game_version.empty() ? "not installed" : _game_version)
                        .c_str());
    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::Text("Installed compatibility pack: unknown version");
    }
    else
    {
        ImGui::Text(fmt::format(
                            "Installed base compatibility pack: {}",
                            _comppack_versions.base.empty() ? "no" : "yes")
                            .c_str());
        ImGui::Text(fmt::format(
                            "Installed patch compatibility pack version: {}",
                            _comppack_versions.patch.empty()
                                    ? "none"
                                    : _comppack_versions.patch)
                            .c_str());
    }

    ImGui::Text(" ");

    printDiagnostic();

    ImGui::Text(" ");

    ImGui::PopTextWrapPos();

    if (_patch_info_fetcher.get_status() == PatchInfoFetcher::Status::Found)
    {
        if (ImGui::Button("Install game and patch###installgame"))
            start_download_package();
    }
    else
    {
        if (ImGui::Button("Install game###installgame"))
            start_download_package();
    }
    ImGui::SetItemDefaultFocus();
    // Ergonomia: quando o primeiro botão está em foco (usuário voltou ao topo
    // da navegação), garante que o scroll da janela volta ao zero.
    if (ImGui::IsItemFocused())
        ImGui::SetScrollY(0.0f);

    if (_base_comppack)
    {
        if (!_downloader->is_in_queue(CompPackBase, _item->titleid))
        {
            if (ImGui::Button("Install base compatibility "
                              "pack###installbasecomppack"))
                start_download_comppack(false);
        }
        else
        {
            if (ImGui::Button("Cancel base compatibility pack "
                              "installation###installbasecomppack"))
                cancel_download_comppacks(false);
        }
    }
    if (_patch_comppack)
    {
        if (!_downloader->is_in_queue(CompPackPatch, _item->titleid))
        {
            if (ImGui::Button(fmt::format(
                                      "Install compatibility pack "
                                      "{}###installpatchcommppack",
                                      _patch_comppack->app_version)
                                      .c_str()))
                start_download_comppack(true);
        }
        else
        {
            if (ImGui::Button("Cancel patch compatibility pack "
                              "installation###installpatchcommppack"))
                cancel_download_comppacks(true);
        }
    }

    // ---- Personal Notes ----
    if (_annotationDb)
    {
        // Check every frame if the virtual keyboard finished
        if (_ime_active && pkgi_dialog_input_update())
        {
            pkgi_dialog_input_get_text(_comment_buf, sizeof(_comment_buf));
            _annotation.comment = _comment_buf;
            _annotationDb->set(_item->titleid, _annotation);
            _item->user_comment = _annotation.comment;
            _ime_active = false;
        }

        ImGui::Separator();
        ImGui::Text("Personal Notes");
        ImGui::Text(" ");

        // Flag picker: saves immediately on click
        ImGui::Text("Flag:");
        for (int fi = 0; fi < UserFlagCount; ++fi)
        {
            const auto f = static_cast<UserFlag>(fi);
            bool active = (_annotation.flag == f);
            if (active)
                ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button(user_flag_label(f)))
            {
                _annotation.flag = f;
                _annotationDb->set(_item->titleid, _annotation);
                _item->user_flag = f;
            }
            if (active)
                ImGui::PopStyleColor();
            if (fi < UserFlagCount - 1)
                ImGui::SameLine();
        }

        ImGui::Text(" ");
        ImGui::Text("Comment:");
        ImGui::TextWrapped(
                "%s",
                _annotation.comment.empty() ? "(no comment)"
                                            : _annotation.comment.c_str());
        ImGui::Text(" ");
        // Button opens the Vita virtual keyboard pre-filled with current comment
        if (ImGui::Button("Edit Comment"))
        {
            pkgi_dialog_input_text("Comment", _comment_buf);
            _ime_active = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Notes"))
        {
            _annotationDb->remove(_item->titleid);
            _annotation = {};
            _comment_buf[0] = '\0';
            _item->user_flag    = UserFlag::None;
            _item->user_comment.clear();
            _ime_active = false;
        }
    }
    // ---- end Personal Notes ----

    ImGui::End();
}

static const auto Red = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
static const auto Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static const auto Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

void GameView::printDiagnostic()
{
    bool ok = true;
    auto const printError = [&](auto const& str)
    {
        ok = false;
        ImGui::TextColored(Red, str);
    };

    auto const systemVersion = pkgi_get_system_version();
    auto const minSystemVersion = get_min_system_version();

    ImGui::Text("Diagnostic:");

    if (systemVersion < minSystemVersion)
    {
        if (!_comppack_versions.present)
        {
            if (_refood_present)
                ImGui::Text("- This game will work thanks to reF00D");
            else if (_0syscall6_present)
                ImGui::Text("- This game will work thanks to 0syscall6");
            else
                printError(
                        "- Your firmware is too old to play this game, you "
                        "must install reF00D or 0syscall6");
        }
    }
    else
    {
        ImGui::Text("- Your firmware is recent enough");
    }

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::TextColored(
                Yellow,
                "- A compatibility pack is installed but not by PKGj, please "
                "make sure it matches the installed version or reinstall it "
                "with PKGj");
        ok = false;
    }

    if (_comppack_versions.base.empty() && !_comppack_versions.patch.empty())
        printError(
                "- You have installed an update compatibility pack without "
                "installing the base pack, install the base pack first and "
                "reinstall the update compatibility pack.");

    std::string comppack_version;
    if (!_comppack_versions.patch.empty())
        comppack_version = _comppack_versions.patch;
    else if (!_comppack_versions.base.empty())
        comppack_version = _comppack_versions.base;

    if (_item->presence == PresenceInstalled && !comppack_version.empty() &&
        comppack_version < _game_version)
        printError(
                "- The version of the game does not match the installed "
                "compatibility pack. If you have updated the game, also "
                "install the update compatibility pack.");

    if (_item->presence == PresenceInstalled &&
        comppack_version > _game_version)
        printError(
                "- The version of the game does not match the installed "
                "compatibility pack. Downgrade to the base compatibility "
                "pack or update the game through the Live Area.");

    if (_item->presence != PresenceInstalled)
    {
        ImGui::Text("- Game not installed");
        ok = false;
    }

    if (ok)
        ImGui::TextColored(Green, "All green");
}

std::string GameView::get_min_system_version()
{
    auto const patchInfo = _patch_info_fetcher.get_patch_info();
    if (patchInfo)
        return patchInfo->fw_version;
    else
        return _item->fw_version;
}

void GameView::refresh()
{
    LOGF("refreshing gameview");
    _refood_present = pkgi_is_module_present("ref00d");
    _0syscall6_present = pkgi_is_module_present("0syscall6");
    _game_version = pkgi_get_game_version(_item->titleid);
    _comppack_versions = pkgi_get_comppack_versions(_item->titleid);
}


void GameView::do_download() {
    pkgi_start_download(*_downloader, *_item);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_package()
{
    if (_item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - already installed", _item->titleid, _item->name);
        pkgi_dialog_question(
        fmt::format(
                "{} is already installed."
                "Would you like to redownload it?",
                _item->name)
                .c_str(),
        {{"Redownload.", [this] { this->do_download(); }},
         {"Dont Redownload.", [] {} }});
        return;
    }
    this->do_download();
}

void GameView::cancel_download_package()
{
    _downloader->remove_from_queue(Game, _item->content);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_comppack(bool patch)
{
    const auto& entry = patch ? _patch_comppack : _base_comppack;

    _downloader->add(DownloadItem{
            patch ? CompPackPatch : CompPackBase,
            _item->name,
            _item->titleid,
            _config->comppack_url + entry->path,
            std::vector<uint8_t>{},
            std::vector<uint8_t>{},
            false,
            "ux0:",
            entry->app_version});
}

void GameView::cancel_download_comppacks(bool patch)
{
    _downloader->remove_from_queue(
            patch ? CompPackPatch : CompPackBase, _item->titleid);
}
