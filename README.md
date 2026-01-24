# UThreads: A High-Performance User-Level Threading Runtime

## Abstract

UThreads is a high-performance, Many-to-Many (M:N) user-level threading library implemented in C++. It maps a large number of lightweight "green threads" onto a fixed pool of kernel-level worker threads, mirroring the concurrency models found in the Go runtime and Java Virtual Threads.

The library is designed to overcome the scalability limitations of standard Operating System threads (pthreads) by moving thread management from kernel-space to user-space. This architecture significantly reduces the overhead of context switching and memory consumption, allowing applications to support high-concurrency workloads—such as network servers handling tens of thousands of connections—on commodity hardware.

## Key Features

* **M:N Multithreading Model:** Decouples logical concurrency from physical parallelism. The runtime schedules $M$ user-level threads across $N$ kernel-level worker threads (typically equal to the number of CPU cores).
* **Preemptive Scheduling:** Implements a time-slicing scheduler using POSIX interval timers (`SIGVTALRM`) to enforce fair CPU distribution and prevent starvation by long-running tasks.
* **Work-Stealing Load Balancer:** Utilizes a randomized work-stealing algorithm to dynamically redistribute tasks from busy cores to idle cores, ensuring optimal CPU utilization.
* **Asynchronous I/O Engine:** Integrates an `epoll`-based event loop to handle blocking system calls. Network I/O operations yield execution immediately, putting the green thread to sleep without blocking the underlying kernel worker.
* **Synchronization Primitives:** Provides a custom `Mutex` implementation that suspends threads (changing state to `BLOCKED`) rather than spin-waiting, preserving CPU cycles for active tasks.
* **Low-Overhead Context Switching:** Leveraging `ucontext_t` for fast register swapping, achieving context switch latencies significantly lower than standard kernel threads.

## System Architecture

The runtime consists of three primary subsystems:

### 1. The Scheduler
The scheduler operates in a decentralized manner. Each kernel worker thread maintains its own local `Ready Queue`.
* **Local Execution:** Workers prioritize tasks from their local queue to maximize cache locality.
* **Work Stealing:** When a worker's local queue is empty, it attempts to lock and steal tasks from the tail of another worker's queue, mitigating load imbalances.

### 2. Context Switching Mechanism
Thread contexts are managed in user-space using the `ucontext` family of functions.
* **State Capture:** The `TCB` (Thread Control Block) stores the instruction pointer, stack pointer, and CPU flags.
* **Switching:** Swapping execution contexts involves only user-level register manipulation, avoiding the expensive mode trap (syscall) required by OS threads.

### 3. Asynchronous Network Poller
To prevent blocking the entire scheduling engine during I/O operations, the runtime intercepts read operations.
* If a socket is not ready (`EAGAIN`), the runtime registers the file descriptor with a global `epoll` instance.
* The calling thread is suspended, and the scheduler swaps in the next available task.
* Once the OS signals data availability via `epoll_wait`, the suspended thread is moved back to the `Ready Queue`.

## Performance Benchmarks

Performance metrics were collected on a quad-core Linux system. The benchmarks compare the UThreads runtime against standard POSIX threads (pthreads).

### Context Switch Latency
Measures the average time required to yield execution from one thread and resume another.

| Implementation | Latency (ns) | Relative Performance |
| :--- | :--- | :--- |
| **UThreads** | **452 ns** | **1.0x (Baseline)** |
| Pthreads (OS) | ~1500 ns | ~3.3x Slower |

**Analysis:** UThreads achieves a ~3x reduction in context switch overhead by eliminating kernel mode traps.

### Multicore Scalability (Throughput)
Measures the execution time of a parallel prime number sieve across varying core counts.

| Cores | Execution Time (s) | Speedup Factor | Efficiency |
| :--- | :--- | :--- | :--- |
| 1 Core | 1.15s | 1.0x | 100% |
| 2 Cores | 0.67s | 1.71x | 85.5% |
| **4 Cores** | **0.40s** | **2.87x** | **71.7%** |

**Analysis:** The system demonstrates near-linear scaling up to 4 cores. The efficiency drop at higher core counts is attributed to contention on the ready queue locks during work-stealing operations, consistent with Amdahl's Law predictions for fine-grained locking.