#include <stdint.h>

int i = 32;

int
main(void)
{
	uint8_t *u8ptr = (uint8_t*)&i;
	return *(u8ptr + 4);
}
