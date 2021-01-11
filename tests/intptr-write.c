int
main(void)
{
	int i = 5;
	int *j = &i;

	*(j + 1) = 1;
	return 0;
}
