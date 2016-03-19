#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include "arm9payload_bin.h"
#include "brahma.h"


s32 main (void) {
	// Initialize services
	//gfxInitDefault();
	//gfxSwapBuffers(); 
	Result res;
	gfxInitDefault();
	gfxSwapBuffers(); 
	
	
	consoleInit(GFX_TOP,NULL);
	//printf("miniPasta2\n\n");

	res=suInit();
	printf("su init: %08X\n",res);

	//res=khaxInit();
	//printf("khax init: %08X\n",res);
	
	res=brahma_init();
	printf("brahma init: %08X\n",res);
	
    load_arm9_payload_from_mem (arm9payload_bin, arm9payload_bin_size);
	printf("payload loaded to RAM\n");
	
	res=firm_reboot();	
	printf("firm reboot: %08X\n",res);
	
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START) break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}
	
	
	
	gfxExit();
	// Return to hbmenu
	return 0;
}
