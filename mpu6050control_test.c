#include "mpu6050control.h"
#include <stdio.h>

int main()
{
	struct mpu6050control mp;

	mpu6050control_start(&mp);
	
	getchar();
	
	mpu6050control_stop(&mp);
	
	return 0;
}
