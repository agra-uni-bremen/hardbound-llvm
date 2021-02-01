struct mystruct {
	char buf[10];
	int val;
};

int
main(void)
{
	struct mystruct s;

	s.buf[10] = '\0';
	return 0;
}
