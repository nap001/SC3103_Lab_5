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
/**
 * set_red_led() - Helper to set the Red LED state
 * @red_fd: File descriptor to the GPIO line handle
 * @state:  1 for ON, 0 for OFF
 */
void set_red_led(int red_fd, int state)
{
    struct gpiohandle_data red_data;
    memset(&red_data, 0, sizeof(red_data));
    red_data.values[0] = state ? 1 : 0;
    ioctl(red_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &red_data);
}

/**
 * monitor_button_and_toggle_red() - Exercise 3.2
 *
 * Monitors the Push Button on GPIO11 using Linux GPIO line event handling
 * (GPIO_GET_LINEEVENT_IOCTL) aligned with Lecture W10L4 Slide 19.
 * When the button press event occurs, it toggles the Red LED (GPIO27):
 *   - OFF -> ON (and remains ON)
 *   - ON -> OFF (and remains OFF)
 */
int monitor_button_and_toggle_red(int chip_fd)
{
    struct gpiohandle_request req_red;
    struct gpioevent_request req_pb;
    struct gpioevent_data event;
    int ret;

    printf("\n=== [Exercise 3.2] Push Button Monitor & Toggle Red LED ===\n");

    // 1. Setup Red LED (GPIO27) as output handle (initially OFF)
    memset(&req_red, 0, sizeof(req_red));
    req_red.lines = 1;
    req_red.lineoffsets[0] = PIN_LED_RED;
    req_red.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req_red.default_values[0] = 0;
    strncpy(req_red.consumer_label, "Pibrella_Red_LED", sizeof(req_red.consumer_label) - 1);

    ret = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req_red);
    if (ret < 0) {
        perror("Failed to request Red LED line handle");
        return -1;
    }
    set_red_led(req_red.fd, 0); // ensure starts OFF

    // 2. Setup Push Button (GPIO11) event request
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

    printf("Red LED is currently OFF.\n");
    printf("Waiting for Push Button press (Press button on Pibrella, 5 presses to finish)...\n");

    // Software debounce: accept a press only if >= 300ms since last valid press.
    // Physical contacts bounce for ~10-30ms generating spurious extra events.
    const uint64_t DEBOUNCE_NS = 300000000ULL;  // 300 ms in nanoseconds
    uint64_t last_accepted_ts = 0;
    int red_state = 0;
    int press_count = 1;
    while (press_count <= 5) {
        // Blocking read on event file descriptor (W10L4 Slide 19)
        ssize_t bytes_read = read(req_pb.fd, &event, sizeof(event));
        if (bytes_read == sizeof(event)) {
            // Discard bounce events that arrive too quickly after last press
            if (last_accepted_ts != 0 &&
                (event.timestamp - last_accepted_ts) < DEBOUNCE_NS) {
                continue;  // bounced event, ignore and wait for next
            }
            last_accepted_ts = event.timestamp;

            // Toggle Red LED state: OFF -> ON, ON -> OFF
            red_state = !red_state;
            set_red_led(req_red.fd, red_state);

            printf("[Event %d/5] Button pressed! Timestamp: %" PRIu64 " ns\n",
                   press_count, event.timestamp);
            printf(" -> Red LED (GPIO%d) switched to %s and remains %s\n\n",
                   PIN_LED_RED, red_state ? "ON" : "OFF", red_state ? "ON" : "OFF");

            ++press_count;
        } else if (bytes_read < 0) {
            if (errno == EINTR) continue; // interrupted by signal
            perror("Error reading button event");
            break;
        }
    }

    // Cleanup handles
    close(req_pb.fd);
    set_red_led(req_red.fd, 0); // turn off on exit
    close(req_red.fd);
    printf("Push Button monitoring completed. Red LED turned OFF.\n");
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

    // Step 4: Run Exercise 3.2 - Monitor Button and Toggle Red LED
    if (monitor_button_and_toggle_red(fd0) != 0) {
        fprintf(stderr, "Exercise 3.2 encountered an error.\n");
    }

    // Step 5: Close chip descriptor
    close(fd0);
    printf("\nAll exercises in gpio.c finished successfully.\n");
    return EXIT_SUCCESS;
}
