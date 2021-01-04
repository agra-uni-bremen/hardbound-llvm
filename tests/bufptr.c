char
myfunc(char *buf, int size)
{
	if (size >= 1)
		return buf[0];
	return '\0';
}

int
main(void)
{
	char buf[4096];
	char *bufptr = buf;

	myfunc(bufptr, sizeof(buf));
	return 0;
}
