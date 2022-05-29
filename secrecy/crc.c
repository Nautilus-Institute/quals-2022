#include "secrecy.h"

uint32_t crc_table[0x100];

ENCRYPT(GenerateCRCTable)
{
	uint32_t x, i, j;

	//generate the table
	for(i = 0; i < 0x100; i++) {
		x = i;
		for(j = 0; j < 8; j++) {
			if(!(x & 1))
				x = ((x >> 1) ^ 0x93a23c59);
			else
				x >>= 1;
				
		}
		crc_table[i] = x;
	}
}

ENCRYPT(crc32)
{
	uint64_t i;
	uint8_t table_entry;
	uint32_t crc_ret = (uint64_t)Param1;
	const uint8_t *data = (const uint8_t *)Param2;
	uint64_t datalen = (uint64_t)Param3;

	for(i = 0; i < datalen; i++) {
		table_entry = crc_ret ^ data[i];
		crc_ret = crc_table[table_entry] ^ (crc_ret >> 8);
	}

	return crc_ret;
}