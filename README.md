A concurrent web server simulation running on FreeRTOS with RISC-V architecture in QEMU. Built to understand how operating systems handle multiple tasks competing for resources.

<img width="648" height="398" alt="image" src="https://github.com/user-attachments/assets/f55a6382-36c4-4448-8fa2-76c9d1842ca6" />

<img width="658" height="394" alt="image" src="https://github.com/user-attachments/assets/a2d3aa9f-0dbf-45ef-bd16-f221af8ca538" />

<img width="563" height="381" alt="image" src="https://github.com/user-attachments/assets/4960a6da-4814-426a-92a2-c049008c699e" />


## What This Does

When you visit a website, your browser sends HTTP requests to a server. But what happens when 100 people visit at the same time? How does the server manage all those requests without dropping any or making everyone wait?

This project simulates that exact problem. Instead of building a real web server with actual networking, we simulate HTTP requests being generated and processed. This way we can focus on the OS concepts, scheduling, queues, synchronization, memory management, and how tasks talk to each other.

The simulation runs on FreeRTOS (a real-time OS used in embedded systems) on RISC-V architecture using QEMU. So the scheduling, context switching, and synchronization aren't fake, they're actually happening in the FreeRTOS kernel.

## Repository Structure

```
freertos-webserver-sim/
├── README.md              
├── main_blinky.c          (web server implementation)
├── main.c                 (modified entry point to call our code)
└── screenshots/           (output examples)
```

Just two source files. `main_blinky.c` has all the web server logic, the tasks, queue handling, statistics. `main.c` is modified to call our code instead of the original blinky demo.

## How It Works

The web server has three types of tasks running concurrently:

**Master Task (Priority 3)**  
Generates 20 HTTP requests. GET and POST to different URLs like `/index.html`, `/api/data`, `/images/logo.png`. Each request has a simulated processing time, 100ms for simple pages, 200ms for images. After creating all requests, this task deletes itself.

**Worker Tasks (Priority 2)**  
Two workers pull requests from a shared queue and process them. Both run at the same priority so FreeRTOS schedules them round-robin. You can see them alternating in the output. They keep running until all requests are done.

**Statistics Task (Priority 1)**  
Runs at lowest priority and displays system stats every 2 seconds. Shows total requests, how many are processed, queue status, free heap memory, and which tasks are currently running. Doesn't interfere with actual work because of its low priority.

Communication happens through a FreeRTOS queue (holds up to 10 requests). A mutex protects the shared counter that tracks total processed requests so workers don't mess up the data when both try to update it at the same time.

## OS Concepts

**Scheduling:** Priority-based preemptive. High priority tasks interrupt low priority ones. Same priority tasks share CPU time.

**IPC:** Queue-based message passing. Classic producer-consumer pattern.

**Synchronization:** Mutex prevents race conditions on shared data.

**Memory Management:** Dynamic allocation from heap. Statistics show real-time memory usage.

**Deadlock Prevention:** Timeouts on all blocking operations. Tasks never wait forever.

**Context Switching:** Task states show when tasks switch between running, blocked, and ready.

## Setup and Running

**Requirements:**
- Linux (Ubuntu or similar)
- RISC-V GCC toolchain (`riscv-none-elf-gcc`)
- QEMU for RISC-V (`qemu-system-riscv32`)

**Clone FreeRTOS:**

Important - you need the `--recurse-submodules` flag otherwise kernel source files won't download.

```bash
git clone --recurse-submodules https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS
```

If you already cloned without it:
```bash
git submodule update --init --recursive
```

**Get Code:**

```bash
git clone https://github.com/cascade77/concurrent-webserver-rtos.git
```

**Replace Files:**

```bash
cd FreeRTOS/FreeRTOS/Demo/RISC-V_RV32_QEMU_VIRT_GCC
cp /path/to/freertos-webserver-sim/main_blinky.c .
cp /path/to/freertos-webserver-sim/main.c .
```

**Fix Makefile:**

The demo Makefile uses `riscv64-unknown-elf-gcc` but most systems have `riscv-none-elf-gcc`:

```bash
cd build/gcc
sed -i 's/riscv64-unknown-elf/riscv-none-elf/g' Makefile
```

**Check Configuration:**

Make sure `main.c` is set to call our code:

```bash
cd ../..
grep "mainCREATE_SIMPLE_BLINKY_DEMO_ONLY" main.c
```

Should show `1`. If it's `0`, change it to `1`.

**Build:**

```bash
cd build/gcc
make clean
make
```

**Run:**

```bash
qemu-system-riscv32 -machine virt -nographic -bios none -kernel output/RTOSDemo.elf
```

Exit QEMU: `Ctrl+A` then `X`

## Common Issues I Faced

**"tasks.c: No such file or directory"**  
This was the first problem I hit. Cloned FreeRTOS without the `--recurse-submodules` flag so the entire Source directory was missing. Had to delete and re-clone properly with submodules.

**"riscv64-unknown-elf-gcc: command not found"**  
The Makefile assumes a different compiler name. Fixed by changing all instances to `riscv-none-elf-gcc` which is what the xPack toolchain uses.

**Old blinky demo runs instead of web server**  
Forgot to set `mainCREATE_SIMPLE_BLINKY_DEMO_ONLY` to 1 in `main.c`. When it's 0, the system runs the full test demo instead of calling `main_blinky()`. Made sure it's 1 and rebuilt.

## Background and Credits

Based on the concurrent web server project from OSTEP (Operating Systems: Three Easy Pieces) by Remzi Arpaci-Dusseau. The original uses POSIX threads to build a real web server. I adapted the concept to FreeRTOS to show the same concurrency ideas in an embedded RTOS environment.

OSTEP Project: https://github.com/remzi-arpacidusseau/ostep-projects/tree/master/concurrency-webserver  
FreeRTOS: https://github.com/FreeRTOS/FreeRTOS

Relevant OSTEP chapters: 26 (Concurrency), 27 (Thread API), 30 (Condition Variables), 31 (Semaphores)

---
