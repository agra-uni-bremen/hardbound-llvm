char
myfunc(char *buf, int size)
{
	return buf[size];
}

int
main(void)
{
	char buf[4096];
	char *bufptr = buf;

	myfunc(bufptr, sizeof(buf));
	return 0;
}
