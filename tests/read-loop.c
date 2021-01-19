int
main(void)
{
	int result = 0;
	int numbers[3] = {1, 2, 3};

	for (int i = 0; i < 4; i++)
		result += numbers[i];

	return result;
}
