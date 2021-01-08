int
myfunc(int *bufptr, int size)
{
	return bufptr[size];
}

int
main(void)
{
	static int buf[3];
	return myfunc(buf, sizeof(buf));
}
