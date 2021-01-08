int
main(void)
{
	int buf[5];
	unsigned int i = 5;
	int *intptr = &buf[i];

	return *(intptr);
}

/*
Must be rewritten tto:

int
main(void)
{
	int buf[5];
	int *bufptr = &buf[0];
	unsigned int i = 5;
	int *intptr = bufptr + i;

	return *(intptr);
}
*/
