static int i = 23;

int
main(void)
{
	int *j = &i;
	return *(j + 1);
}
