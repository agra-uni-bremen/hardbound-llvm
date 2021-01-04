static int buf[5];

int
main(void)
{
	int *intptr = &buf[3];
	return *(intptr + 2);
}
