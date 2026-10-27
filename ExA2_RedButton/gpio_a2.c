/**
 * gpio_a2.c - SC3103 Practical Exercise #5 (Exercise A2)
 *
 * Demonstration:
 *   Monitor the Push Button (GPIO11). Each time it is pressed, the Red LED (GPIO27)
 *   toggles state:
 *     - OFF -> ON (and remains ON)
 *     - ON -> OFF (and remains OFF)
 *   Waits for 5 button presses then exits.
 *
 * Pibrella Pin Connections:
 *   - Red LED:    GPIO27 (Header Pin 13) - Output (initially OFF)
 *   - Push Button:GPIO11 (Header Pin 23) - Input, Active-High
 *                 Circuit: 3V3 -> 1K -> Button -> GPIO11
 *                 Press = LOW->HIGH = Rising Edge
 *
 * Key API (W10L4 Slide 19):
 *   - gpioevent_request with GPIOEVENT_REQUEST_RISING_EDGE
 *   - GPIO_GET_LINEEVENT_IOCTL
 *   - Blocking read() on event fd (no polling)
 *   - Software timestamp debounce to ignore mechanical contact bounce (< 300 ms)
 *
 * Compilation on Raspberry Pi:
 *   gcc -Wall -O2 -o gpio_a2 gpio_a2.c
 *
 * Execution (requires root to open /dev/gpiochip0):
 *   sudo ./gpio_a2
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GPIO_CHIP_DEV "/dev/gpiochip0"
#define PIN_LED_RED 27
#define PIN_PUSH_BUTTON 11
#define NUM_PRESSES 5            /* number of button presses before exit */
#define DEBOUNCE_NS 300000000ULL /* 300 ms minimum gap between valid presses */

/* ---- Helper: set the Red LED state (0 = OFF, 1 = ON) ---- */
static int set_red_led(int red_fd, int state) {
  struct gpiohandle_data d;
  memset(&d, 0, sizeof(d));
  d.values[0] = state ? 1 : 0;
  return ioctl(red_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &d);
}

int main(int argc, char *argv[]) {
  int fd0, ret;
  struct gpiochip_info cinfo;
  struct gpiohandle_request req_red;
  struct gpioevent_request req_pb;
  struct gpioevent_data event;

  printf("=== SC3103 Exercise A2: Push Button -> Toggle Red LED (ON/OFF) ===\n");

  /* ---- Step 1: Open GPIO chip ---- */
  fd0 = open(GPIO_CHIP_DEV, O_RDWR);
  if (fd0 < 0) {
    perror("Failed to open " GPIO_CHIP_DEV " (run with sudo)");
    return EXIT_FAILURE;
  }

  /* ---- Step 2: Query chip info ---- */
  memset(&cinfo, 0, sizeof(cinfo));
  if (ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo) < 0) {
    perror("GPIO_GET_CHIPINFO_IOCTL failed");
    close(fd0);
    return EXIT_FAILURE;
  }
  printf("GPIO chip: %s, \"%s\", %u lines\n\n", cinfo.name, cinfo.label,
         cinfo.lines);

  /* ---- Step 3: Claim Red LED (GPIO27) as output (initially OFF) ---- */
  memset(&req_red, 0, sizeof(req_red));
  req_red.lines = 1;
  req_red.lineoffsets[0] = PIN_LED_RED;
  req_red.flags = GPIOHANDLE_REQUEST_OUTPUT;
  req_red.default_values[0] = 0; /* start OFF */
  strncpy(req_red.consumer_label, "ExA2_Red_LED",
          sizeof(req_red.consumer_label) - 1);

  ret = ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_red);
  if (ret < 0) {
    perror("Failed to get Red LED line handle");
    close(fd0);
    return EXIT_FAILURE;
  }
  set_red_led(req_red.fd, 0); /* Ensure Red LED starts OFF */

  /* ---- Step 4: Register Push Button (GPIO11) rising-edge event ----
   * W10L4 Slide 19: GPIOEVENT_REQUEST_RISING_EDGE
   * Button is Active-High -> press generates LOW->HIGH (rising) edge. */
  memset(&req_pb, 0, sizeof(req_pb));
  req_pb.lineoffset = PIN_PUSH_BUTTON;
  req_pb.handleflags = GPIOHANDLE_REQUEST_INPUT;
  req_pb.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
  strncpy(req_pb.consumer_label, "ExA2_PushButton",
          sizeof(req_pb.consumer_label) - 1);

  ret = ioctl(fd0, GPIO_GET_LINEEVENT_IOCTL, &req_pb);
  if (ret < 0) {
    perror("Failed to register Push Button event");
    close(req_red.fd);
    close(fd0);
    return EXIT_FAILURE;
  }

  printf(
      "Red LED is currently OFF.\n"
      "Waiting for %d button press(es) on GPIO%d (Pibrella red button)...\n"
      "  - Press 1: LED switches from OFF to ON and remains ON\n"
      "  - Press 2: LED switches from ON to OFF and remains OFF\n\n",
      NUM_PRESSES, PIN_PUSH_BUTTON);

  /* ---- Step 5: Event loop - blocking read, no polling ----
   * Software debounce via timestamp: physical contacts bounce for ~10-30 ms,
   * generating multiple rapid rising-edge events queued in the kernel buffer.
   * We accept a press only if at least DEBOUNCE_NS (300 ms) has elapsed
   * since the last accepted press. event.timestamp is in nanoseconds. */
  uint64_t last_accepted_ts = 0;
  int red_state = 0; /* tracks current LED state: 0 = OFF, 1 = ON */
  int press = 1;
  while (press <= NUM_PRESSES) {
    ssize_t n = read(req_pb.fd, &event, sizeof(event));
    if (n == (ssize_t)sizeof(event)) {
      /* Discard bounce: event arrived too soon after last valid press */
      if (last_accepted_ts != 0 &&
          (event.timestamp - last_accepted_ts) < DEBOUNCE_NS) {
        printf("  [Bounce filtered: delta %" PRIu64 " ms < 300 ms, ignored]\n",
               (event.timestamp - last_accepted_ts) / 1000000ULL);
        continue; /* go back to read(), wait for next event */
      }
      last_accepted_ts = event.timestamp;

      /* Toggle LED state: OFF -> ON (remains ON) or ON -> OFF (remains OFF) */
      red_state = !red_state;
      set_red_led(req_red.fd, red_state);

      printf("[Press %d/%d] Button detected! Timestamp: %" PRIu64 " ns\n",
             press, NUM_PRESSES, event.timestamp);
      printf("  -> Red LED (GPIO%d) switched to %s and remains %s\n\n",
             PIN_LED_RED, red_state ? "ON" : "OFF", red_state ? "ON" : "OFF");
      ++press;
    } else if (n < 0) {
      if (errno == EINTR)
        continue; /* signal interrupt, retry */
      perror("Error reading button event");
      break;
    }
  }

  /* ---- Step 6: Cleanup ---- */
  close(req_pb.fd);
  /* Turn OFF LED before exiting */
  set_red_led(req_red.fd, 0);
  close(req_red.fd);
  close(fd0);

  printf("Exercise A2 complete. Red LED turned OFF and cleaned up.\n");
  return EXIT_SUCCESS;
}
