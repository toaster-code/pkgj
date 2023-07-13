#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <fmt/format.h>

#include "psx.hpp"
#include "log.hpp"

static std::map<std::string, std::string> installed_psx_games;


bool pkgi_is_psx_game_installed_titleid(std::string title_id) {
	return (installed_psx_games.find(title_id) != installed_psx_games.end());
}

void pkgi_process_pbp(std::string eboot_pbp, std::string disc_id) {
	pbp_header pbp_hdr;
	
	SceUID fd = sceIoOpen(eboot_pbp.c_str(), SCE_O_RDONLY, 0777);	
	int read_sz = sceIoRead(fd, &pbp_hdr, sizeof(pbp_header));

	// check got actual pbp header sz read
	if(read_sz >= sizeof(pbp_header));
	{
		// check magic is PBP magic
		if(memcmp(pbp_hdr.magic, "\0PBP", sizeof(pbp_hdr.magic)) == 0){
			sceIoLseek(fd, pbp_hdr.data_psar, SCE_SEEK_SET);
			LOGF("pbp_hdr magic is \"\\0PBP\"");
			
			char magic[0x8];
			read_sz = sceIoRead(fd, magic, sizeof(magic));
			if(read_sz >= sizeof(magic)) {
				
				// check is psx game
				if(memcmp(magic, "PSISOIMG", sizeof(magic)) == 0 || memcmp(magic, "PSTITLEI", sizeof(magic)) == 0) {
					LOGF("data.psar magic is {}", std::string(magic, 8));
					
					np_data_psp data_psp;
					sceIoLseek(fd, pbp_hdr.data_psp, SCE_SEEK_SET);
					read_sz = sceIoRead(fd, &data_psp, sizeof(np_data_psp));
					if(read_sz >= sizeof(np_data_psp)) {
						int cid_sz = strnlen(data_psp.content_id, 40);
						if(cid_sz == 36) {
							std::string title_id = std::string(data_psp.content_id + 7, 9);
							installed_psx_games.insert({title_id, disc_id});
							LOGF("Inserting {} = {}", title_id, disc_id);
						}
					}
				}
			}
			
		}
	}
	
	sceIoClose(fd);
	
}

void pkgi_scan_pbps() {
	std::string parent_folder = "ux0:/pspemu/PSP/GAME";
	SceUID dfd = sceIoDopen(parent_folder.c_str());
	
	std::string eboot_file;
	
	int dir_read_ret = 0;
	SceIoDirent dir;
	int ret = 0;
	do{
		memset(&dir, 0x00, sizeof(SceIoDirent));
		dir_read_ret = sceIoDread(dfd, &dir);	

		std::string disc_id = std::string(dir.d_name);
		std::string eboot_file = fmt::format("{}/{}/EBOOT.PBP", parent_folder, disc_id);
		pkgi_process_pbp(eboot_file, disc_id);
		
		LOGF("checking {} .", eboot_file);
		
	} while(dir_read_ret > 0);
	
	sceIoDclose(dfd);
	
}
