#define BUFFER_SIZE 4096
static char buf[BUFFER_SIZE];

char
myfunc(char *buf, int size)
{
	return buf[size];
}

int
main(void)
{
	char *bufptr = buf;
	myfunc(bufptr, BUFFER_SIZE);
	return 0;
}
