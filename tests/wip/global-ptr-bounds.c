int buf[5];
int *ptr = &buf[0];

int
main(void)
{
	return *(ptr + 5);
}
