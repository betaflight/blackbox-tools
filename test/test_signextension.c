#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "../src/tools.h"

int main(void)
{
	for (int32_t i = -8388608; i < 8388608; i++) {
		uint32_t u24 = i & 0x00FFFFFF;

		assert(i == signExtend24Bit(u24));
	}

	for (int32_t i = -8192; i < 8192; i++) {
		uint16_t u14 = i & 0x3FFF;

		assert(i == signExtend14Bit(u14));
	}

	for (int32_t i = -32; i < 32; i++) {
		uint32_t u6 = i & 0x3F;

		assert(i == signExtend6Bit(u6));
	}

	for (int32_t i = -8; i < 8; i++) {
		uint32_t u4 = i & 0x0F;

		assert(i == signExtend4Bit(u4));
	}

	for (int32_t i = -2; i < 2; i++) {
		uint32_t u2 = i & 0x00FFFFFF;

		assert(i == signExtend2Bit(u2));
	}

	printf("Done");

	return 0;
}
