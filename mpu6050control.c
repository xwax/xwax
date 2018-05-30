#include "mpu6050control.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#pragma pack(push,1)

struct buff_t
{
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t t;
	unsigned char c;
};

#pragma pack(pop)

int chksum(int a, int b, unsigned char* buf, size_t len)
{
	int res=( a + b ) & 0xff;
	for(int i=0;i<len;++i) res = (res + buf[i]) & 0xff;
	return res;
}

void mpu6050control_main(void* arg)
{
	struct mpu6050control* mpuc=(struct mpu6050control*)arg;
	
    struct sockaddr_rc addr = { 0 };
    int s, status;
//    char dest[18] = "20:17:12:04:51:21";
//    char dest[18] = "20:18:01:09:36:12";

	fprintf(stderr, "Connecting to %s\n", mpuc->btmac);
	
	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) 1;
    str2ba( mpuc->btmac, &addr.rc_bdaddr );

    status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

	unsigned char c;
	struct buff_t buff;
	
	while(!mpuc->finished)
	{
		recv(s, &c, 1, 0);
		if (c==0x55)
		{
			recv(s, &c, 1, 0);
			if (c==0x52)
			{
				recv(s, &buff, sizeof(struct buff_t), 0);	
				double angv_z = buff.z / 32768.0 * 2000.0;
				if (chksum(0x55,0x52,(unsigned char*)&buff,sizeof(struct buff_t)-1) == buff.c)
				{
					mpuc->angv = angv_z;
//					fprintf(stderr,"angz = %lf %x =? %x\n", angv_z, chksum(0x55,0x52,(unsigned char*)&buff,sizeof(struct buff_t)-1), buff.c);
				}
//				printf("angz = %lf %x =? %x\n", angv_z, t, chksum(0x55,0x52,(unsigned char*)&buff,sizeof(struct buff_t)-1), buff.c);
			}
		}
	}
    close(s);
}

static void* mpu6050control_launch(void *p)
{
    mpu6050control_main(p);
    return NULL;
}


int mpu6050control_start(struct mpu6050control* mpuc)
{
	printf("mpu6050control_start");
	
	mpuc->finished=false;
	int r = pthread_create(&mpuc->ph,NULL,mpu6050control_launch,(void*)mpuc);
	if (r != 0) {
            perror("pthread_create");
            return -1;
        }
	return 0;
}

void mpu6050control_stop(struct mpu6050control* mpuc)
{
	mpuc->finished=true;
	if (pthread_join(mpuc->ph, NULL) != 0)
            abort();
	printf("mpu6050control_stop");
}

double mpu6050control_get_pitch(struct mpu6050control* mpuc)
{
	double pitch = -mpuc->angv/200.0;
//	fprintf(stderr, "xxx %lf\n", pitch);
	return pitch;
}

