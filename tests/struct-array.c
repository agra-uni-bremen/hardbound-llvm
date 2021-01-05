struct mystruct {
	int a;
	long long b;
};

int
main(void) {
	struct mystruct structs[5];
	struct mystruct *ptr = &structs[0];

	return (ptr + 6)->a;
}
