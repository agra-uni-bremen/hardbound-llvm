int
main(void)
{
	char buf[3];
	int buflen = sizeof(buf);

	for (int i = 0; i < buflen; i++)
		buf[i + 1] = '\0';

	return 0;
}
