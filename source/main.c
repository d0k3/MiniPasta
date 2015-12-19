#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include "arm9payload_bin.h"
#include "brahma.h"

s32 main (void) {
	// Initialize services
	gfxInitDefault();
	gfxSwapBuffers(); 
	//consoleInit(GFX_TOP,NULL);
	
	brahma_init();
	
	
    load_arm9_payload_from_mem (arm9payload_bin, arm9payload_bin_size);
	firm_reboot();	
	
	gfxExit();
	// Return to hbmenu
	return 0;
}
