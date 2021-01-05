#include <stdint.h>

/* struct which requires padding */
struct mystruct {
	uint32_t a;
	uint64_t b;
};

int
main(void) {
	struct mystruct structs[5];
	struct mystruct *ptr = &structs[0];

	return (ptr + 5)->a;
}
