int
main(void)
{
	int buf[5];
	int *intptr = &buf[3];

	return *(intptr + 2);
}
