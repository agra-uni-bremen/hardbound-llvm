int
main(void)
{
	int buf[5];
	unsigned int i = 3;
	int *intptr = &buf[i];

	return *(intptr + 2);
}
