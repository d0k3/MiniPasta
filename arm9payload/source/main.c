#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "common.h"

int patchFIRM(u32 p1, u32 p2){
	
	u8 patch1[]={0x00, 0x20, 0x3B, 0xE0};
	u8 patch2[]={0x00, 0x20, 0x08, 0xE0};
	
	memcpy((u32*)p1,patch1,4);
	memcpy((u32*)p2,patch2,4);
	return 0;

}

int check(u32 a,u32 b){
	if( *(u32*)a==0x6869000C && *(u32*)b==0x0028000E){
		patchFIRM(a,b);
		return 1;
	}
	else 
	{
		return 0;
	}
	
}

int main() {

	
	int result=0;
	int i=0;
	u32 patch1[]={0x080549C4, 0x08051650, 0x0805164C, 0x0805235C, 0x080521C4, 0x080521C8, 0x080523C4, 0x0805235C};
	u32 patch2[]={0x0804239C, 0x080570D8, 0x080570D4, 0x08057FE4, 0x08057E98, 0x08057E9C, 0x08058098, 0x08058100};
	

	for(i=0;i<8;i++){
		result=check(patch1[i],patch2[i]);
		if(result){
			break;
		}
	}
	
	if(!result){                      //new 3ds 9.0-9.2
	    if(*(u32*)0x08052FD8==0xFF9C0061){
			*(u32*)0x08052FD8=0x77CE206D;
			*(u32*)0x08058804=0xC173C55A;
		 }
		 result=1;
	}


	return 0;    // return control to FIRM ARM9 code (performs firmlaunch)
}