#include <stdio.h>

int main()
{
	FILE * fd;
	fd = fopen("test.txt", "w");
	fclose(fd);

	printf("Hello World");

	return 0;
}
