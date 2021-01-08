int
main(void)
{
	char buf[3];
	char *bufptr = &buf[0];

	*(bufptr + sizeof(buf)) = '\0';
	return 0;
}
