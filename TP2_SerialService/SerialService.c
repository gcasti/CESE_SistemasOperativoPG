#include <stdio.h>
#include <stdlib.h>
#include "SerialManager.h"


int main(void)
{
	printf("Inicio Serial Service\r\n");
	
	if(serial_open(1,115200)!=0)
	{
		printf("Error abriendo puerto serie \r\n");
	}

	exit(EXIT_SUCCESS);
	return 0;
}
