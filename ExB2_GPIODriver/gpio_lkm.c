/**
 * gpio_lkm.c - SC3103 Practical Exercise #5 (Exercise B2)
 *
 * Description:
 *   Loadable Kernel Module (LKM) GPIO Device Driver for Raspberry Pi using
 *   the Integer-based GPIO interface (<linux/gpio.h>) and Kernel IRQ handling.
 *
 * Functionality:
 *   - Requests GPIO4 (Green LED on Pibrella) and sets it as output (initially ON).
 *   - Requests GPIO11 (Push Button on Pibrella) and sets it as input with 1000us debounce.
 *   - Maps GPIO11 to an IRQ line via gpio_to_irq().
 *   - Registers an Interrupt Service Routine (rpi_gpio_isr) triggered on rising edge.
 *   - Toggles Green LED state each time button interrupt fires.
 *   - Performs full resource cleanup upon module unload (free_irq, gpio_unexport, gpio_free).
 *
 * References:
 *   - Lab Manual Exercise 4.2 & Appendix C
 *   - Lecture W10L4 Slides 77-82
 *
 * Demo Steps on RPi:
 *   1. Build:        make
 *   2. Load:         sudo insmod gpio_lkm.ko
 *   3. Check dmesg:  dmesg | tail      -> "Initializing the GPIO LKM"
 *                                      -> "Button mapped to IRQ: <N>"
 *   4. Hardware:     Green LED is initially OFF.
 *                    - Press button: Green LED switches from OFF to ON and remains ON.
 *                    - Press button: Green LED switches from ON to OFF and remains OFF.
 *   5. Check dmesg:  dmesg | tail      -> "GPIO Interrupt! Push button pressed. Green LED switched to ON/OFF"
 *   6. Verify IRQ:   cat /proc/interrupts | grep rpi_gpio_handler
 *   7. Verify sysfs: ls /sys/class/gpio/
 *   8. Unload:       sudo rmmod gpio_lkm
 *   9. Check dmesg:  dmesg | tail      -> "Goodbye from the GPIO LKM!"
 */

#include <linux/init.h>      // Macros __init, __exit, module_init, module_exit
#include <linux/module.h>    // Core kernel module structures and macros
#include <linux/kernel.h>    // printk() and log priority levels
#include <linux/gpio.h>      // Integer-based GPIO functions (gpio_request, gpio_direction_*, etc.)
#include <linux/interrupt.h> // IRQ functions (request_irq, free_irq, IRQF_TRIGGER_*)
#include <linux/jiffies.h>   // jiffies, msecs_to_jiffies() for software debounce

// Pin definitions on Pibrella
static unsigned int ledGreen   = 4;  // GPIO4 (Pin 7) connected to Green LED
static unsigned int pushButton = 11; // GPIO11 (Pin 23) connected to Push Button
static unsigned int irqNumber;       // Shared IRQ number for the push button
static bool ledOn = 0;               // Tracks current state of the Green LED (0 = OFF, 1 = ON)
static unsigned long last_interrupt_time = 0; // Timestamp for debounce filtering

/**
 * rpi_gpio_isr() - Interrupt Service Routine (ISR) for GPIO push button
 * @irq:    The interrupt number being handled
 * @dev_id: Private data passed to request_irq (NULL here)
 * @regs:   Saved processor registers (struct pt_regs *)
 *
 * Triggered whenever the Push Button rising-edge transition occurs.
 * Uses 300ms software debounce to ignore physical switch contact bounce.
 * Toggles LED state: OFF -> ON (remains ON), ON -> OFF (remains OFF).
 */
static irq_handler_t rpi_gpio_isr(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long diff = jiffies - last_interrupt_time;

    // Discard bounce interrupts arriving within 300ms of last valid press
    if (diff < msecs_to_jiffies(300)) {
        return (irq_handler_t) IRQ_HANDLED;
    }
    last_interrupt_time = jiffies;

    // Toggle the LED state: OFF -> ON, ON -> OFF
    ledOn = !ledOn;

    // Set LED GPIO hardware line accordingly
    gpio_set_value(ledGreen, ledOn);

    printk(KERN_ALERT "GPIO Interrupt! Push button pressed. Green LED switched to %s\n",
           ledOn ? "ON" : "OFF");

    // Announce to kernel that IRQ has been successfully handled
    return (irq_handler_t) IRQ_HANDLED;
}

/**
 * rpi_gpio_init() - Module Initialization Function
 *
 * Claims GPIOs, configures direction, exports to sysfs, maps IRQ,
 * and attaches the interrupt handler.
 */
static int __init rpi_gpio_init(void)
{
    int result = 0;

    printk(KERN_ALERT "Initializing the GPIO LKM\n");

    // Validate GPIO numbers
    if (!gpio_is_valid(ledGreen)) {
        printk(KERN_ALERT "GPIO LKM: Invalid LED GPIO %u\n", ledGreen);
        return -ENODEV;
    }
    if (!gpio_is_valid(pushButton)) {
        printk(KERN_ALERT "GPIO LKM: Invalid Button GPIO %u\n", pushButton);
        return -ENODEV;
    }

    // Step 1: Claim and setup Green LED (GPIO4) - initially OFF
    ledOn = false; // Default state for LED is OFF
    result = gpio_request(ledGreen, "sysfs");
    if (result) {
        printk(KERN_ALERT "GPIO LKM: Failed to request GPIO %u (err: %d)\n", ledGreen, result);
        return result;
    }
    gpio_direction_output(ledGreen, ledOn); // Set as output and initialize to OFF
    gpio_export(ledGreen, false);           // Export to /sys/class/gpio; false prevents user direction change

    // Step 2: Claim and setup Push Button (GPIO11)
    result = gpio_request(pushButton, "sysfs");
    if (result) {
        printk(KERN_ALERT "GPIO LKM: Failed to request GPIO %u (err: %d)\n", pushButton, result);
        gpio_unexport(ledGreen);
        gpio_free(ledGreen);
        return result;
    }
    gpio_direction_input(pushButton);         // Set as input
    gpio_set_debounce(pushButton, 1000);      // Debounce delay of 1000 microseconds
    gpio_export(pushButton, false);           // Export to /sys/class/gpio

    // Step 3: Map Push Button GPIO to an IRQ number
    irqNumber = gpio_to_irq(pushButton);
    printk(KERN_ALERT "Button mapped to IRQ: %d\n", irqNumber);

    // Step 4: Request interrupt line
    result = request_irq(
        irqNumber,                          // Interrupt number requested
        (irq_handler_t) rpi_gpio_isr,       // ISR handler function
        IRQF_TRIGGER_RISING,                // Trigger on rising edge
        "rpi_gpio_handler",                 // Line label in /proc/interrupts
        NULL                                // *dev_id for shared interrupt lines (NULL for exclusive)
    );

    if (result) {
        printk(KERN_ALERT "GPIO LKM: Failed to request IRQ %u (err: %d)\n", irqNumber, result);
        gpio_unexport(pushButton);
        gpio_free(pushButton);
        gpio_unexport(ledGreen);
        gpio_free(ledGreen);
        return result;
    }

    printk(KERN_ALERT "GPIO LKM: Initialization completed successfully.\n");
    return 0;
}

/**
 * rpi_gpio_exit() - Module Cleanup Function
 *
 * Turns off LED, frees IRQ line, unexports sysfs entries, and releases GPIOs.
 */
static void __exit rpi_gpio_exit(void)
{
    // Turn LED off
    gpio_set_value(ledGreen, 0);

    // Unexport GPIOs from sysfs
    gpio_unexport(ledGreen);
    gpio_unexport(pushButton);

    // Free the registered IRQ
    free_irq(irqNumber, NULL);

    // Free GPIO ownership back to kernel
    gpio_free(ledGreen);
    gpio_free(pushButton);

    printk(KERN_ALERT "Goodbye from the GPIO LKM!\n");
}

// Module registration macros
module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit);

// Module metadata macros
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SC3103");
MODULE_DESCRIPTION("Raspberry Pi GPIO Interrupt LKM Driver");
MODULE_VERSION("V1");
