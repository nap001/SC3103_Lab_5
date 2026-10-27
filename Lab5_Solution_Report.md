# SC3103 Embedded Programming - Practical Exercise #5
## GPIO Interface Access through User Space and Kernel Space (Raspberry Pi)

---

## 1. Overview & Learning Objectives

This practical exercise investigates two fundamental paradigms for accessing hardware General Purpose Input/Output (GPIO) lines on an embedded Linux platform (Raspberry Pi 3 with Pibrella add-on board):
1. **User Space Access (Exercise A)**: Interfacing via Linux descriptor-based character device files (`/dev/gpiochip0`) using standard POSIX system calls (`open()`, `ioctl()`, `read()`, `close()`).
2. **Kernel Space Access (Exercise B)**: Interfacing directly via Loadable Kernel Modules (LKMs) utilizing the integer-based GPIO API (`<linux/gpio.h>`) and hardware interrupt handling (`<linux/interrupt.h>`).

### Pibrella Hardware Pin Mapping
From the lab manual circuit schematic (Page 3) and lecture notes (W10L4):
- **Green LED**: RPi `GPIO4` (Header Pin 7) - Output
- **Yellow LED**: RPi `GPIO17` (Header Pin 11) - Output
- **Red LED**: RPi `GPIO27` (Header Pin 13) - Output
- **Push Button**: RPi `GPIO11` (Header Pin 23) - Input (Active-High: 3V3 -> 1K -> Push Button -> GPIO11)
- **Piezo Buzzer**: RPi `GPIO18` (Header Pin 12) - Output

---

## 2. Exercise A: User Space Access of GPIO

### 2.1 Analysis & Code (`gpio.c`)
In modern Linux (v4.8+), the legacy sysfs integer interface (`/sys/class/gpio/`) is superseded by the **descriptor-based character device interface** (`/dev/gpiochip*`).

#### Key System Calls & Data Structures (from `linux/gpio.h`):
- `int fd = open("/dev/gpiochip0", O_RDWR)`: Opens the GPIO controller character device node.
- `ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &cinfo)`: Queries chip metadata (name, label, total lines = 54 on BCM2835/BCM2837).
- `struct gpiohandle_request`: Configures one or multiple GPIO lines simultaneously as input or output.
- `struct gpiohandle_data`: Holds line output/input values as a bit/integer array (`values[0..lines-1]`).
- `ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data)`: Sets the digital output levels.
- `struct gpioevent_request` & `struct gpioevent_data`: Configures edge detection (Rising/Falling) and waits via blocking `read()` for hardware events with nanosecond timestamps.

#### Bugs & Omissions in Appendix A Sample Code:
1. **Missing standard includes**: Appendix A omits `<stdio.h>`, `<stdlib.h>`, `<unistd.h>`, `<fcntl.h>`, and `<sys/ioctl.h>`.
2. **Typo in data array**: `data_GY.value[1]` was written instead of `data_GY.values[1]`.
3. **Missing cleanup**: When exiting, turning off LEDs before releasing file descriptors prevents floating hardware states.

### 2.2 Push Button Circuit & Edge Detection (Exercise 3.2):
The Pibrella push button circuit connects:
`3V3 -> 1K Resistor -> Push Button Switch -> GPIO11`
- **Unpressed (Idle)**: The switch is open. The Raspberry Pi's internal pull-down resistor pulls `GPIO11` to ground, so it reads **LOW (`0`)**.
- **Pressed**: The switch closes, directly connecting `3V3` (via the 1K protection resistor) to `GPIO11`, so it reads **HIGH (`1`)**.
- **Therefore, the button is Active-HIGH**:
  - **Press event**: LOW $\rightarrow$ HIGH transition (**RISING EDGE**).
  - **Release event**: HIGH $\rightarrow$ LOW transition (**FALLING EDGE**).

This explains why **Lecture W10L4** and the lab manual explicitly configure:
- Slide 19: `req_pb.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;`
- Slide 82 & Lab Manual Appendix C: `IRQF_TRIGGER_RISING, // on rising edge (press, not release)`

In accordance with Lecture W10L4 Slide 19, event monitoring is handled asynchronously and efficiently via `GPIOEVENT_REQUEST_RISING_EDGE`, blocking with `read()` without wasting CPU cycles in busy-wait polling loops.

### 2.3 User-Space Program Compilation & Execution
On the Raspberry Pi terminal:
```bash
# Compile
gcc -Wall -O2 -o gpio gpio.c

# Execute (requires root privileges to open /dev/gpiochip0)
sudo ./gpio
```

---

## 3. Exercise B: Kernel Space Access of GPIO

### 3.1 Part 4.1: Simple Hello World LKM (`hello_lkm.c`)

#### Code Analysis:
```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init hello_init(void) {
    printk(KERN_ALERT "Hello from kernel world\n");
    return 0; // 0 for success
}

static void __exit hello_exit(void) {
    printk(KERN_ALERT "Goodbye from kernel world\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CE3103");
MODULE_DESCRIPTION("Simple Hello module");
MODULE_VERSION("V1");
```

#### Bugs & Omissions in Appendix B:
1. `module_init(xxxx);` and `module_exit(xxxx);` had placeholder `xxxx`, which must be filled with `hello_init` and `hello_exit`.
2. `Static void __exit` has an incorrect uppercase `Static`. In C, keyword is strictly lowercase `static`.
3. Typo in quotes: Smart quotes `“CE3103"` and `“V1"` cause compiler syntax errors and must be ASCII quotes `"`.

#### Why `printk()` instead of `printf()`?
Kernel space operates in an isolated environment without access to C standard libraries (`glibc`). `printk()` writes directly to the kernel circular log buffer (`ring buffer`) and prepends log priority levels (e.g., `<1>` for `KERN_ALERT`, `<6>` for `KERN_INFO`).

---

### 3.2 Part 4.2: GPIO Device Driver LKM (`gpio_lkm.c`)

#### Analysis of Appendix C Errors & Required Additions:
1. **Missing Headers**: Appendix C omitted `#include <linux/init.h>` and `#include <linux/kernel.h>`.
2. **ISR Incompleteness**: The body of `rpi_gpio_isr()` was omitted (`:`). It must toggle `ledOn = !ledOn;` and call `gpio_set_value(ledGreen, ledOn);`.
3. **Syntax Error in printk**: `printk(KERN_ALERT "GPIO Interrupt!\n"));` had an extra trailing parenthesis.
4. **Identifier Typos**:
   - `ledGren` in `gpio_direction_output` -> corrected to `ledGreen`.
   - `rpi_gpia_isr` in `request_irq` -> corrected to `rpi_gpio_isr`.
   - `ledgreen` in `gpio_free` -> corrected to `ledGreen` (case-sensitive).
   - `pushbutton` in `gpio_free` -> corrected to `pushButton`.
5. **Missing Module Registration & Metadata**: Appendix C omitted `module_init(rpi_gpio_init)`, `module_exit(rpi_gpio_exit)`, `MODULE_LICENSE("GPL")`, etc.
6. **Best Practice Exporting (W10L4 Slides 81-82)**:
   - Call `gpio_export(ledGreen, false)` and `gpio_export(pushButton, false)` so that the pins appear under `/sys/class/gpio/` while preventing user-space direction changes (`false`).
   - Symmetrically call `gpio_unexport()` before `gpio_free()` in the exit handler.
7. **Safe Debouncing**: `gpio_set_debounce(pushButton, 1000)` filters mechanical button contact bounce.

---

## 4. Compilation & Execution Guide for Kernel Modules

### 4.1 Kbuild Out-Of-Tree Makefile
The kernel requires a specialized `kbuild` Makefile to link against running kernel headers:
```makefile
obj-m += hello_lkm.o
obj-m += gpio_lkm.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: modules user

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: gpio.c
	gcc -Wall -O2 -o gpio gpio.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f gpio
```

### 4.2 Step-by-Step Testing Procedure

#### Step 1: Build Modules
```bash
make
```
This produces the binary module objects `hello_lkm.ko` and `gpio_lkm.ko`.

#### Step 2: Test `hello_lkm.ko`
```bash
# Insert module
sudo insmod hello_lkm.ko

# Verify module is loaded
lsmod | grep hello

# View initialization message in kernel log
dmesg | tail -n 5
# Output: Hello from kernel world

# Inspect module metadata
modinfo hello_lkm.ko

# Remove module
sudo rmmod hello_lkm

# View cleanup message
dmesg | tail -n 5
# Output: Goodbye from kernel world
```

#### Step 3: Test `gpio_lkm.ko`
```bash
# Insert GPIO module
sudo insmod gpio_lkm.ko

# Check dmesg for IRQ allocation
dmesg | tail -n 10
# Output: Initializing the GPIO LKM
#         Button mapped to IRQ: <irqNumber>

# Verify interrupt registration in procfs
cat /proc/interrupts | grep rpi_gpio_handler

# Verify sysfs exports
ls -l /sys/class/gpio/

# Hardware Test:
# 1. Observe that Green LED is initially ON.
# 2. Press the push button on Pibrella board.
# 3. Observe Green LED toggles OFF -> ON -> OFF on each press.
# 4. Check kernel log for interrupt trigger events:
dmesg | tail -n 10

# Unload module
sudo rmmod gpio_lkm

# Verify LED turns off and exit message is logged
dmesg | tail -n 5
# Output: Goodbye from the GPIO LKM!
```

---

## 5. Answers to Core Conceptual Questions (Aligned with W10L4)

### Q1: What is the fundamental difference between User Space and Kernel Space?
- **User Space (Ring 3)**: Unprivileged execution mode where normal applications run. Processes have restricted direct access to hardware and memory. To perform hardware operations or access system resources, user applications must request kernel services via **system calls** (e.g., `open()`, `ioctl()`, `read()`).
- **Kernel Space (Ring 0)**: Privileged execution mode where the OS core and device drivers run. It has direct, unrestricted access to the CPU registers, physical memory, and hardware peripherals. Errors here can cause kernel panics and system crashes.

### Q2: Why use Loadable Kernel Modules (LKMs) rather than building everything into a monolithic kernel?
1. **Dynamic Extensibility**: Modules can be loaded (`insmod`) and unloaded (`rmmod`) dynamically into the running kernel on-demand without rebooting the system.
2. **Memory Efficiency**: Keeps the resident kernel image small, only consuming RAM for active drivers.
3. **Faster Development Cycle**: Eliminates the slow cycle of full kernel reconfiguration, recompilation, and system reboot.

### Q3: What is the significance of the `__init` and `__exit` macros in LKMs?
- `__init`: Informs the compiler that the function is only used during initialization. Once initialization completes, the kernel frees that code segment from RAM to conserve memory.
- `__exit`: Informs the kernel that the function is only needed for cleanup during module removal. If the driver is statically built into the kernel (not loadable), `__exit` code is discarded entirely at build time.

### Q4: Why must kernel modules use `copy_to_user()` and `copy_from_user()` instead of direct pointer dereferencing?
User-space memory addresses are virtual addresses in a separate process address space and may be:
- Paged out to swap space (causing page faults).
- Invalid or malicious pointers trying to exploit kernel memory access.
Direct pointer dereference in kernel mode bypasses MMU protection checks and could crash the kernel. `copy_to_user()` and `copy_from_user()` validate page tables, check read/write permissions, and handle faults safely.

### Q5: What are Device Nodes, Major Numbers, and Minor Numbers?
- **Device Node**: A special file located in `/dev` representing a device interface (e.g. `crw-rw---- 1 root dialout 4, 64 /dev/ttyS0`).
- **Major Number**: Identifies the specific device driver associated with the device. The kernel uses the major number to look up the driver's `file_operations` in its internal registry.
- **Minor Number**: Used internally by the driver to distinguish between different physical devices or instances managed by the same driver.

### Q6: Why is `gpio_set_debounce()` important for push buttons?
Mechanical switches suffer from contact bounce: when pressed or released, the physical contacts mechanically chatter for several milliseconds, generating multiple rapid electrical transitions. Without debouncing, a single physical button press triggers multiple spurious interrupts. `gpio_set_debounce()` applies hardware or software filtering so that only a stable transition triggers the ISR.

### Q7: Why is descriptor-based GPIO (`/dev/gpiochip*`) preferred over legacy integer-based sysfs GPIO (`/sys/class/gpio/`)?
- **Performance**: Sysfs requires string formatting, open/write/close file operations for each state change. Descriptor-based char devices use binary `ioctl()` commands and keep open file descriptors.
- **Atomic Batch Operations**: Descriptor-based interface allows reading or setting multiple GPIO lines simultaneously in a single `ioctl()` call.
- **Hardware Event Handling**: Directly supports event subscriptions (`gpioevent_request`) with microsecond/nanosecond kernel timestamps and blocking event queues, eliminating CPU-heavy polling.
- **Security & Concurrency**: Clear line ownership prevents competing applications from altering pin configuration while claimed.
