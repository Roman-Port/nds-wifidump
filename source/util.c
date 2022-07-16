#include "defines.h"

#include <nds.h>

int query_gba_rom_size()
{
	if(*(vu32*)(0x08000004) != 0x51AEFF24)
		return 0;
	int i;
	for(i = (1<<20); i < (1<<25); i<<=1)
	{
		vu16 *rompos = (vu16*)(0x08000000+i);
		int j;
		bool romend = true;
		for(j = 0; j < 0x1000; j++)
		{
			if(rompos[j] != j)
			{
				romend = false;
				break;
			}
		}
		if(romend) break;
	}
	return i;
}

void safe_string(const char* input, int length, char* output, int allowEarlyEnd) {
    for (int i = 0; i < length; i++) {
        if (allowEarlyEnd && input[i] == 0) {
            output[i] = 0;
            break;
        }
        if (input[i] >= ' ' && input[i] <= '~')
            output[i] = input[i];
        else
            output[i] = '_';
    }
}