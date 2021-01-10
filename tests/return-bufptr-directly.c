static int buf[4096];

int *
getbuf(void)
{
	return &buf[0];
}

int
main(void)
{
	int *bufptr = getbuf();
	return *(bufptr + 4096);
}
