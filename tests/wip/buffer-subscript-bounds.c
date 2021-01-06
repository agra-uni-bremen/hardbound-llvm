int
main(void)
{
	int buf[5];
	unsigned int i = 5;
	int *intptr = &buf[i];

	return *(intptr);
}
