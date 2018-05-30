#include <pthread.h>
#include <stdbool.h>

struct mpu6050control
{
	char btmac[18];
	double angv;
	pthread_t ph;
	bool finished;
};

int mpu6050control_start(struct mpu6050control* mpuc);
void mpu6050control_stop(struct mpu6050control* mpuc);

double mpu6050control_get_pitch(struct mpu6050control* mpuc);
