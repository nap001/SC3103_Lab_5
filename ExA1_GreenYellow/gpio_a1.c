/**
 * gpio_a1.c - SC3103 Practical Exercise #5 (Exercise A1)
 *
 * Demonstration:
 *   Blink Green LED (GPIO4) and Yellow LED (GPIO17) alternately for 5 cycles
 *   (1 second ON per LED per cycle, ~10 seconds total runtime).
 *
 * Pibrella Pin Connections:
 *   - Green LED:  GPIO4  (Header Pin 7)
 *   - Yellow LED: GPIO17 (Header Pin 11)
 *
 * Compilation on Raspberry Pi:
 *   gcc -Wall -O2 -o gpio_a1 gpio_a1.c
 *
 * Execution (requires root to open /dev/gpiochip0):
 *   sudo ./gpio_a1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

#define GPIO_CHIP_DEV    "/dev/gpiochip0"
#define PIN_LED_GREEN    4
#define PIN_LED_YELLOW   17

int main(int argc, char *argv[])
{
    int fd0, ret;
    struct gpiochip_info    cinfo;
    struct gpiohandle_request req_GY;
    struct gpiohandle_data    data_GY;

    printf("=== SC3103 Exercise A1: Green & Yellow LED Blink ===\n");

    /* ---- Step 1: Open GPIO chip ---- */
    fd0 = open(GPIO_CHIP_DEV, O_RDWR);
    if (fd0 < 0) {
        perror("Failed to open " GPIO_CHIP_DEV " (run with sudo)");
        return EXIT_FAILURE;
    }

    /* ---- Step 2: Query chip info ---- */
    memset(&cinfo, 0, sizeof(cinfo));
    ret = ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo);
    if (ret < 0) {
        perror("GPIO_GET_CHIPINFO_IOCTL failed");
        close(fd0);
        return EXIT_FAILURE;
    }
    printf("GPIO chip: %s, \"%s\", %u lines\n\n",
           cinfo.name, cinfo.label, cinfo.lines);

    /* ---- Step 3: Request Green + Yellow as outputs ---- */
    memset(&req_GY, 0, sizeof(req_GY));
    req_GY.lines            = 2;
    req_GY.lineoffsets[0]   = PIN_LED_GREEN;   /* index 0 -> Green  */
    req_GY.lineoffsets[1]   = PIN_LED_YELLOW;  /* index 1 -> Yellow */
    req_GY.flags            = GPIOHANDLE_REQUEST_OUTPUT;
    strncpy(req_GY.consumer_label, "ExA1_GY_LEDs",
            sizeof(req_GY.consumer_label) - 1);

    ret = ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_GY);
    if (ret < 0) {
        perror("GPIO_GET_LINEHANDLE_IOCTL failed");
        close(fd0);
        return EXIT_FAILURE;
    }

    /* ---- Step 4: Blink alternately for 5 cycles ---- */
    /* Start: Green ON, Yellow OFF */
    memset(&data_GY, 0, sizeof(data_GY));
    data_GY.values[0] = 1;   /* Green  ON  */
    data_GY.values[1] = 0;   /* Yellow OFF */

    for (int i = 0; i < 5; ++i) {
        printf("Cycle %d/5:  Green = %-3s  |  Yellow = %s\n",
               i + 1,
               data_GY.values[0] ? "ON"  : "OFF",
               data_GY.values[1] ? "ON"  : "OFF");

        ret = ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
        if (ret < 0) {
            perror("GPIOHANDLE_SET_LINE_VALUES_IOCTL failed");
            close(req_GY.fd);
            close(fd0);
            return EXIT_FAILURE;
        }

        usleep(1000000);   /* 1 second */

        /* Toggle both LEDs for next cycle */
        data_GY.values[0] = !data_GY.values[0];
        data_GY.values[1] = !data_GY.values[1];
    }

    /* ---- Step 5: Turn off LEDs and release resources ---- */
    data_GY.values[0] = 0;
    data_GY.values[1] = 0;
    ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);

    close(req_GY.fd);
    close(fd0);

    printf("\nExercise A1 complete. Both LEDs are now OFF.\n");
    return EXIT_SUCCESS;
}
