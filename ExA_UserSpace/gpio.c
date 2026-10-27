/**
 * gpio.c - SC3103 Practical Exercise #5 (Exercise A: Simultaneous A1 & A2)
 *
 * Requirements:
 *   - A1 & A2 are ongoing at the same time (concurrently).
 *   - Only the Green LED (GPIO4) blinks for 5 seconds (each blink is ~0.5s: 250ms ON / 250ms OFF).
 *   - The Push Button (GPIO11) uses polling (GPIOHANDLE_GET_LINE_VALUES_IOCTL) to detect presses.
 *   - Pressing the button toggles the Red LED (GPIO27):
 *       OFF -> ON (and remains ON)
 *       ON -> OFF (and remains OFF)
 *
 * Pibrella Pin Connections:
 *   - Green LED:   GPIO4  (Pin 7)  - Output (blinks for 5s)
 *   - Red LED:     GPIO27 (Pin 13) - Output (toggles on button press, starts OFF)
 *   - Push Button: GPIO11 (Pin 23) - Input, Active-High (polled at 100 Hz)
 *
 * Compilation on Raspberry Pi:
 *   gcc -Wall -O2 -o gpio gpio.c
 *
 * Execution (requires root privileges):
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
#include <time.h>

#define GPIO_CHIP_DEV "/dev/gpiochip0"

// Pin definitions on Pibrella
#define PIN_LED_GREEN    4
#define PIN_LED_RED      27
#define PIN_PUSH_BUTTON  11

#define TOTAL_DURATION_SEC    5.0    /* 5 seconds total run time */
#define BLINK_HALF_PERIOD_MS  250    /* 250ms ON + 250ms OFF = 0.5s per blink cycle */
#define POLL_INTERVAL_US      10000  /* 10 ms sleep per loop iteration (100 Hz polling) */
#define DEBOUNCE_MS           200    /* 200 ms debounce filter for button press */

/* ---- Helper: get monotonic time in milliseconds ---- */
static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ---- Helper: write output value to a line handle ---- */
static int set_line_value(int line_fd, int val)
{
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    data.values[0] = val ? 1 : 0;
    return ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}

/* ---- Helper: read input value from a line handle (polling) ---- */
static int get_line_value(int line_fd)
{
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        return -1;
    }
    return data.values[0];
}

int main(int argc, char *argv[])
{
    int fd0, ret;
    struct gpiochip_info cinfo;
    struct gpiohandle_request req_green;
    struct gpiohandle_request req_red;
    struct gpiohandle_request req_btn;

    printf("===============================================================\n");
    printf("  SC3103 Exercise A: Simultaneous A1 & A2 (Concurrent Execution)\n");
    printf("===============================================================\n");
    printf("Description:\n");
    printf("  - A1: Green LED (GPIO%d) blinks for 5.0 seconds (~0.5s per blink)\n", PIN_LED_GREEN);
    printf("  - A2: Push Button (GPIO%d) is polled at 100 Hz to toggle Red LED (GPIO%d)\n",
           PIN_PUSH_BUTTON, PIN_LED_RED);
    printf("  - Both tasks run concurrently in a non-blocking polling loop.\n\n");

    /* Step 1: Open GPIO character device */
    fd0 = open(GPIO_CHIP_DEV, O_RDWR);
    if (fd0 < 0) {
        perror("Failed to open " GPIO_CHIP_DEV " (run with sudo)");
        return EXIT_FAILURE;
    }

    /* Step 2: Query chip information */
    memset(&cinfo, 0, sizeof(cinfo));
    ret = ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo);
    if (ret < 0) {
        perror("GPIO_GET_CHIPINFO_IOCTL failed");
        close(fd0);
        return EXIT_FAILURE;
    }
    printf("GPIO chip: %s, \"%s\", %u lines\n\n", cinfo.name, cinfo.label, cinfo.lines);

    /* Step 3: Claim Green LED (GPIO4) as output (starts ON) */
    memset(&req_green, 0, sizeof(req_green));
    req_green.lines = 1;
    req_green.lineoffsets[0] = PIN_LED_GREEN;
    req_green.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req_green.default_values[0] = 1;
    strncpy(req_green.consumer_label, "Green_LED_A1", sizeof(req_green.consumer_label) - 1);
    ret = ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_green);
    if (ret < 0) {
        perror("Failed to request Green LED handle");
        close(fd0);
        return EXIT_FAILURE;
    }

    /* Step 4: Claim Red LED (GPIO27) as output (starts OFF) */
    memset(&req_red, 0, sizeof(req_red));
    req_red.lines = 1;
    req_red.lineoffsets[0] = PIN_LED_RED;
    req_red.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req_red.default_values[0] = 0;
    strncpy(req_red.consumer_label, "Red_LED_A2", sizeof(req_red.consumer_label) - 1);
    ret = ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_red);
    if (ret < 0) {
        perror("Failed to request Red LED handle");
        close(req_green.fd);
        close(fd0);
        return EXIT_FAILURE;
    }

    /* Step 5: Claim Push Button (GPIO11) as input for POLLING */
    memset(&req_btn, 0, sizeof(req_btn));
    req_btn.lines = 1;
    req_btn.lineoffsets[0] = PIN_PUSH_BUTTON;
    req_btn.flags = GPIOHANDLE_REQUEST_INPUT;
    strncpy(req_btn.consumer_label, "Button_Poll_A2", sizeof(req_btn.consumer_label) - 1);
    ret = ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_btn);
    if (ret < 0) {
        perror("Failed to request Push Button handle");
        close(req_red.fd);
        close(req_green.fd);
        close(fd0);
        return EXIT_FAILURE;
    }

    /* Initialize initial LED states */
    int green_state = 1; // start with Green LED ON
    set_line_value(req_green.fd, green_state);

    int red_state = 0;   // start with Red LED OFF
    set_line_value(req_red.fd, red_state);

    uint64_t start_ms = get_time_ms();
    uint64_t last_green_toggle_ms = start_ms;
    uint64_t last_btn_press_ms = 0;
    int prev_btn_val = 0;
    int toggle_count = 0;

    printf("[0.00s] Started! Green LED is ON (blinking). Red LED is OFF.\n");
    printf("        Press the Pibrella button at any time during the 5 seconds!\n\n");

    const uint64_t total_duration_ms = (uint64_t)(TOTAL_DURATION_SEC * 1000.0);

    /* Step 6: Non-blocking concurrent execution loop */
    while (1) {
        uint64_t now_ms = get_time_ms();
        uint64_t elapsed_ms = now_ms - start_ms;

        if (elapsed_ms >= total_duration_ms) {
            break; /* 5 seconds completed */
        }

        /* ---- Task A1: Toggle Green LED every 250 ms (0.5s per blink cycle) ---- */
        if (now_ms - last_green_toggle_ms >= BLINK_HALF_PERIOD_MS) {
            green_state = !green_state;
            set_line_value(req_green.fd, green_state);
            last_green_toggle_ms = now_ms;
        }

        /* ---- Task A2: Poll Push Button (GPIO11) and toggle Red LED (GPIO27) ---- */
        int btn_val = get_line_value(req_btn.fd);
        if (btn_val >= 0) {
            /* Detect button press transition: 0 (released) -> 1 (pressed) */
            if (btn_val == 1 && prev_btn_val == 0) {
                /* Debounce check: ignore bounce within DEBOUNCE_MS */
                if (now_ms - last_btn_press_ms >= DEBOUNCE_MS) {
                    last_btn_press_ms = now_ms;
                    red_state = !red_state;
                    set_line_value(req_red.fd, red_state);
                    toggle_count++;
                    printf("[Time: %4.2fs] [POLLED] Button pressed! Red LED toggled: %s (remains %s)\n",
                           (double)elapsed_ms / 1000.0,
                           red_state ? "ON" : "OFF",
                           red_state ? "ON" : "OFF");
                }
            }
            prev_btn_val = btn_val;
        }

        /* Sleep 10 ms (100 Hz sampling tick - non-blocking time-slicing) */
        usleep(POLL_INTERVAL_US);
    }

    printf("\n[5.00s] 5.0 seconds completed! Green LED finished blinking.\n");
    printf("        Total button presses registered: %d\n", toggle_count);
    printf("        Final Red LED state was: %s\n", red_state ? "ON" : "OFF");

    /* Step 7: Cleanup */
    set_line_value(req_green.fd, 0); // Turn off Green LED
    set_line_value(req_red.fd, 0);   // Turn off Red LED
    close(req_btn.fd);
    close(req_red.fd);
    close(req_green.fd);
    close(fd0);

    printf("        Both LEDs turned OFF. Handles closed. Exercise A done.\n");
    return EXIT_SUCCESS;
}
