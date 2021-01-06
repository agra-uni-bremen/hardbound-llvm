int
main(void)
{
	int matrix[2][2];
	int *ptr = &matrix[0][0];

	return *(ptr + 4);
}
