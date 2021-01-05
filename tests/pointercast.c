#include <stdint.h>

uint32_t i = 32;

int
main(void)
{
	uint8_t *u8ptr = (uint8_t*)&i;
	return *(u8ptr + 4);
}
