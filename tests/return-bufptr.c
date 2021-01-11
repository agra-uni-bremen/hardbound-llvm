static char buf[4096];

char *
getbuf(void)
{
	char *bufptr = buf;
	return bufptr;
}

int
main(void)
{
	char *bufptr = getbuf();
	bufptr[4096] = '\0';
	return 0;
}
