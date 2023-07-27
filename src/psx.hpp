#pragma once
#include <cstdint>
#include <string>

typedef struct pbp_header
{
    char magic[0x4];
    uint32_t version;
    uint32_t param_sfo;    
    uint32_t icon0_png;    
    uint32_t icon1_pmf;
    uint32_t pic0_png;
    uint32_t pic1_png;    
    uint32_t snd0_at3;    
    uint32_t data_psp;    
    uint32_t data_psar;

} pbp_header;

typedef struct np_data_psp{
    char magic[0x4];
    char unk[0x7C];
    char unk2[0x32];
    char unk3[0xE];
    char hash[0x14];
    char reserved[0x58];
    char unk4[0x434];
    char content_id[0x30];
} np_data_psp;

void pkgi_scan_pbps();
std::string pkgi_pbp_read_disc_id(std::string eboot_pbp);
bool pkgi_is_psx_game_installed_titleid(std::string title_id);
void pkgi_psx_add_installed_game(std::string title_id, std::string disc_id);