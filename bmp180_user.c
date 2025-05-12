#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#define BMP180_GET_TEMP_REAL   _IOR(0xB1, 2, int)
#define BMP180_GET_PRESS_REAL  _IOR(0xB1, 3, int)

int main() {
    int fd = open("/dev/bmp180", O_RDWR);
    if (fd < 0) {
        perror("Open /dev/bmp180 failed");
        return 1;
    }

    int temp, press;

    if (ioctl(fd, BMP180_GET_TEMP_REAL, &temp) == 0)
        printf("Temperature: %d.%d °C\n", temp / 10, abs(temp % 10));
    else
        perror("ioctl - temp");

    if (ioctl(fd, BMP180_GET_PRESS_REAL, &press) == 0)
        printf("Pressure: %d.%d hPa\n", press / 100, abs(press % 100));
    else
        perror("ioctl - press");

    close(fd);
    return 0;
}

