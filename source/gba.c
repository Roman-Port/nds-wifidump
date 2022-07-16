#include "defines.h"

const gbasav_type_t gbasav_types[] = {
    {
        .name = "NONE",
        .size = 0
    },
    {
        .name = "EEPROM",
        .size = 512
    },
    {
        .name = "EEPROM",
        .size = 8 * 1024
    },
    {
        .name = "SRAM",
        .size = 32 * 1024
    },
    {
        .name = "FLASH",
        .size = 64 * 1024
    },
    {
        .name = "FLASH",
        .size = 128 * 1024
    }
};

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define MAGIC_EEPR 0x52504545
#define MAGIC_SRAM 0x4d415253
#define MAGIC_FLAS 0x53414c46

#define MAGIC_H1M_ 0x5f4d3148

//don't fully understand this; https://github.com/AdmiralCurtiss/nds-savegame-manager/blob/master/arm9/source/gba.cpp
int gbasav_identify() {
    uint32 *data = (uint32*)0x08000000;
	for (int i = 0; i < (0x02000000 >> 2); i++, data++) {
		if (*data == MAGIC_EEPR)
			return 2; // TODO: Try to figure out 512 bytes version...
		if (*data == MAGIC_SRAM)
			return 3;
		if (*data == MAGIC_FLAS) {
			uint32 *data2 = data + 1;
			if (*data2 == MAGIC_H1M_)
				return 5;
			else
				return 4;
		}
	}
	return 0;
}

//don't fully understand this; https://github.com/AdmiralCurtiss/nds-savegame-manager/blob/master/arm9/source/gba.cpp
void gbasav_read(u8 *dst, u32 src, u32 len, int type)
{
	int nbanks = 2; // for type 4,5
	
	switch (type) {
	case 3: {
		// SRAM: blind copy
		int start = 0x0a000000 + src;
		u8 *tmpsrc = (u8*)start;
		sysSetBusOwners(true, true);
		for (u32 i = 0; i < len; i++, tmpsrc++, dst++)
			*dst = *tmpsrc;
		break;
		}
	case 4:
		// FLASH - must be opend by register magic, then blind copy
		nbanks = 1;
	case 5:
		for (int j = 0; j < nbanks; j++) {
			// we need to wait a few cycles before the hardware reacts!
			*(u8*)0x0a005555 = 0xaa;
			swiDelay(10);
			*(u8*)0x0a002aaa = 0x55;
			swiDelay(10);
			*(u8*)0x0a005555 = 0xb0;
			swiDelay(10);
			*(u8*)0x0a000000 = (u8)j;
			swiDelay(10);
			u32 start, sublen;
			if (j == 0) {
				start = 0x0a000000 + src;
				sublen = (src < 0x10000) ? min(len, (1 << 16) - src) : 0;
			} else if (j == 1) {
				start = max(0x09ff0000 + src, 0x0a000000);
				sublen = (src + len < 0x10000) ? 0 : min(len, len - (0x10000 - src));
			}
			u8 *tmpsrc = (u8*)start;
			sysSetBusOwners(true, true);
			for (u32 i = 0; i < sublen; i++, tmpsrc++, dst++)
				*dst = *tmpsrc;
		}
		break;
	}
}