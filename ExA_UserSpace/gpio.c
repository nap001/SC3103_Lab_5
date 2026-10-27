/**
 * gpio.c - SC3103 Practical Exercise #5 (Exercise A: 3.1 & 3.2)
 *
 * Description:
 *   User space GPIO access using Linux descriptor-based character device interface
 *   (/dev/gpiochip0) via ioctl().
 *
 * Requirements:
 *   - 3.1: Blink Green LED (GPIO4) and Yellow LED (GPIO17) on the Pibrella board.
 *   - 3.2: Monitor the Push Button (GPIO11) and blink the Red LED (GPIO27) whenever
 *          the Push Button is pressed.
 *
 * Pibrella Pin Connections:
 *   - Green LED:   GPIO4
 *   - Yellow LED:  GPIO17
 *   - Red LED:     GPIO27
 *   - Push Button: GPIO11 (Active-High circuit: 3V3 -> 1K -> Button -> GPIO11)
 *
 * Compilation on Raspberry Pi:
 *   gcc -Wall -O2 -o gpio gpio.c
 *
 * Execution:
 *   sudo ./gpio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <inttypes.h>

#define GPIO_CHIP_DEV "/dev/gpiochip0"

// Pin definitions according to Pibrella schematic
#define PIN_LED_GREEN   4
#define PIN_LED_YELLOW  17
#define PIN_LED_RED     27
#define PIN_PUSH_BUTTON 11

/**
 * blink_green_yellow() - Exercise 3.1
 * Blinks Green and Yellow LEDs alternately for 5 cycles (1 second per toggle).
 */
int blink_green_yellow(int chip_fd)
{
    struct gpiohandle_request req_GY;
    struct gpiohandle_data data_GY;
    int ret;

    printf("\n=== [Exercise 3.1] Blinking Green and Yellow LEDs ===\n");

    memset(&req_GY, 0, sizeof(req_GY));
    memset(&data_GY, 0, sizeof(data_GY));

    // Request 2 lines as output
    req_GY.lines = 2;
    req_GY.lineoffsets[0] = PIN_LED_GREEN;   // index 0 -> Green LED
    req_GY.lineoffsets[1] = PIN_LED_YELLOW;  // index 1 -> Yellow LED
    req_GY.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strncpy(req_GY.consumer_label, "Pibrella_GY_LEDs", sizeof(req_GY.consumer_label) - 1);

    // Initial states: Green = 1 (ON), Yellow = 0 (OFF)
    data_GY.values[0] = 1;
    data_GY.values[1] = 0;

    ret = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req_GY);
    if (ret < 0) {
        perror("Failed to get line handle for Green & Yellow LEDs");
        return -1;
    }

    for (int i = 0; i < 5; ++i) {
        printf("Cycle %d/5: Green = %s, Yellow = %s\n",
               i + 1,
               data_GY.values[0] ? "ON" : "OFF",
               data_GY.values[1] ? "ON" : "OFF");

        ret = ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
        if (ret < 0) {
            perror("Failed to set line values for Green & Yellow LEDs");
            close(req_GY.fd);
            return -1;
        }

        // Sleep for 1 second (1,000,000 microseconds)
        usleep(1000000);

        // Toggle LED values for the next cycle
        data_GY.values[0] = !data_GY.values[0];
        data_GY.values[1] = !data_GY.values[1];
    }

    // Turn off both LEDs before closing
    data_GY.values[0] = 0;
    data_GY.values[1] = 0;
    ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);

    // Release line handle
    close(req_GY.fd);
    printf("Green and Yellow blink completed.\n");
    return 0;
}

/**
 * blink_red_led() - Helper to blink the Red LED for a specified duration
 */
void blink_red_led(int red_fd, int count, useconds_t delay_us)
{
    struct gpiohandle_data red_data;
    memset(&red_data, 0, sizeof(red_data));

    for (int i = 0; i < count; ++i) {
        // Red LED ON
        red_data.values[0] = 1;
        ioctl(red_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &red_data);
        usleep(delay_us);

        // Red LED OFF
        red_data.values[0] = 0;
        ioctl(red_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &red_data);
        usleep(delay_us);
    }
}

/**
 * monitor_button_and_blink_red() - Exercise 3.2
 *
 * Monitors the Push Button on GPIO11 using Linux GPIO line event handling
 * (GPIO_GET_LINEEVENT_IOCTL) aligned with Lecture W10L4 Slide 19.
 * When the button press event occurs, it triggers the Red LED (GPIO27) to blink.
 */
int monitor_button_and_blink_red(int chip_fd)
{
    struct gpiohandle_request req_red;
    struct gpioevent_request req_pb;
    struct gpioevent_data event;
    int ret;

    printf("\n=== [Exercise 3.2] Push Button Monitor & Red LED Blink ===\n");

    // 1. Setup Red LED (GPIO27) as output handle
    memset(&req_red, 0, sizeof(req_red));
    req_red.lines = 1;
    req_red.lineoffsets[0] = PIN_LED_RED;
    req_red.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strncpy(req_red.consumer_label, "Pibrella_Red_LED", sizeof(req_red.consumer_label) - 1);

    ret = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req_red);
    if (ret < 0) {
        perror("Failed to request Red LED line handle");
        return -1;
    }

    // 2. Setup Push Button (GPIO11) event request
    // On Pibrella, the push button is wired as Active-High:
    // 3V3 -> 1K Resistor -> Push Button -> GPIO11.
    // - Unpressed: Switch is open -> GPIO11 is pulled LOW (0) via internal pulldown.
    // - Pressed: Switch closes -> connects 3V3 to GPIO11 (reads HIGH / 1).
    // Hence, pressing the button produces a LOW-to-HIGH transition (RISING EDGE),
    // precisely as configured in W10L4 Slide 19 (GPIOEVENT_REQUEST_RISING_EDGE)
    // and Slide 82 ("on rising edge (press, not release)").
    memset(&req_pb, 0, sizeof(req_pb));
    req_pb.lineoffset = PIN_PUSH_BUTTON;
    req_pb.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req_pb.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    strncpy(req_pb.consumer_label, "Push Button Event", sizeof(req_pb.consumer_label) - 1);

    ret = ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req_pb);
    if (ret < 0) {
        perror("Failed to request Push Button event handler");
        close(req_red.fd);
        return -1;
    }

    printf("Waiting for Push Button press (Press button on Pibrella, 5 presses to finish)...\n");

    for (int press_count = 1; press_count <= 5; ++press_count) {
        // Blocking read on event file descriptor (W10L4 Slide 19)
        ssize_t bytes_read = read(req_pb.fd, &event, sizeof(event));
        if (bytes_read == sizeof(event)) {
            printf("[Event %d] Button pressed! Timestamp: %" PRIu64 " ns\n",
                   press_count, event.timestamp);
            printf(" -> Blinking Red LED (GPIO27) 3 times...\n");

            // Blink Red LED 3 times (150ms ON, 150ms OFF)
            blink_red_led(req_red.fd, 3, 150000);
        } else if (bytes_read < 0) {
            if (errno == EINTR) continue; // interrupted by signal
            perror("Error reading button event");
            break;
        }
    }

    // Cleanup handles
    close(req_pb.fd);
    close(req_red.fd);
    printf("Push Button monitoring completed.\n");
    return 0;
}

int main(int argc, char *argv[])
{
    int fd0;
    struct gpiochip_info cinfo;
    int ret;

    printf("SC3103 Practical Exercise #5 - User Space GPIO Control\n");
    printf("Opening GPIO chip descriptor: %s\n", GPIO_CHIP_DEV);

    // Step 1: Open GPIO character device
    fd0 = open(GPIO_CHIP_DEV, O_RDWR);
    if (fd0 < 0) {
        perror("Failed to open " GPIO_CHIP_DEV " (make sure you have sudo/root privileges)");
        return EXIT_FAILURE;
    }

    // Step 2: Query chip information using ioctl
    memset(&cinfo, 0, sizeof(cinfo));
    ret = ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo);
    if (ret < 0) {
        perror("GPIO_GET_CHIPINFO_IOCTL failed");
        close(fd0);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "GPIO chip 0: %s, \"%s\", %u lines\n",
            cinfo.name, cinfo.label, cinfo.lines);

    // Step 3: Run Exercise 3.1 - Blink Green and Yellow LEDs
    if (blink_green_yellow(fd0) != 0) {
        fprintf(stderr, "Exercise 3.1 encountered an error.\n");
    }

    // Step 4: Run Exercise 3.2 - Monitor Button and Blink Red LED
    if (monitor_button_and_blink_red(fd0) != 0) {
        fprintf(stderr, "Exercise 3.2 encountered an error.\n");
    }

    // Step 5: Close chip descriptor
    close(fd0);
    printf("\nAll exercises in gpio.c finished successfully.\n");
    return EXIT_SUCCESS;
}
