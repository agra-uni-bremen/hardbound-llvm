static char buf[4096];

char *
getbuf(void)
{
	return buf;
}

int
main(void)
{
	char *bufptr = getbuf();
	bufptr[4096] = '\0';
	return 0;
}
