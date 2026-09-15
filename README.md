# CS3.301 Operating Systems and Networks: Mini Project 1

This repository contains the complete implementation for **Mini Project 1** of CS3.301 (Operating Systems and Networks). The project consists of two main parts:
1. **C-Shell**: A feature-rich custom POSIX-compliant C shell.
2. **xv6 MLFQ Scheduler**: An implementation of a Multi-Level Feedback Queue (MLFQ) CPU scheduler inside the xv6 RISC-V operating system kernel.

---

## 📁 Repository Structure

```text
mp1/
├── c-shell/
│   ├── src/             # Source files (.c) for lexer, parser, built-in commands & execution
│   ├── include/         # Header files (.h) defining data structures & function signatures
│   └── Makefile         # Build script compiling to shell.out with gcc POSIX flags
├── xv6/
│   ├── user/            # User-space utilities and schedulertest program
│   ├── mkfs/            # Disk image generator
│   ├── kernel/          # Kernel code (proc.c, trap.c, proc.h, etc.)
│   ├── Makefile         # xv6 build script supporting SCHEDULER=MLFQ
│   └── report.md        # Detailed MLFQ analysis & scheduler comparison report
├── AI-usage.pdf         # AI tool usage documentation and prompt history
└── README.md            # Master project README file
```

---

## 🐚 Part 1: C-Shell

The C-Shell is built from scratch in C using standard POSIX system calls. It features a modular architecture separating the lexer, parser, built-in intrinsic functions, redirection/piping logic, and process control.

### Key Features Implemented

#### Part A: Shell Input & Parsing
- **Custom Prompt**: Displays `<username@hostname:currentpath>`. Automatically substitutes the initial shell startup directory with `~`.
- **Lexer & Grammar Parser**: Uses regular grammar rules to tokenize input up to 1024 characters.
  - Supports single quotes (`'...'`), double quotes (`"..."`), and escape characters (`\`).
  - Implements **maximal munch** token matching.
  - Validates token sequences and rejects invalid syntax with `cshell: invalid syntax`.

#### Part B: Shell Intrinsics
- **`hop`**: Directory navigation supporting `~`, `.`, `..`, `-`, and relative/absolute paths. Includes persistent **frecency tracking** (recency + frequency ranking algorithm) for fuzzy directory hopping.
- **`reveal`**: Lists directory contents in ASCII lexicographical order. Supports flags `-a` (show hidden files) and `-t` (recursive traversal with trailing `/` on directories).
- **`peek`**: Concatenates and displays file contents with support for line numbering (`-n`), reverse order (`-r`), or stdin reading.
- **`locate`**: Searches for executable pathnames in the current working directory first, followed by directories in `$PATH`.

#### Part C: Redirection and Pipes
- **File Redirection**: Supports multi-file input redirection (`<`), output truncation (`>`), and output appending (`>>`).
- **Piping**: Connects command pipelines of arbitrary length (`cmd1 | cmd2 | ... | cmdN`) using `pipe()` and `dup2()`.

#### Part D: Execution Management
- **Sequential Execution**: Executes semicolon-separated commands (`cmd1 ; cmd2`).
- **Background Execution**: Runs background processes terminated with `&` without blocking. Automatically reaps exited background jobs using an asynchronous `SIGCHLD` handler (`waitpid(..., WNOHANG)`).

#### Part E: Exotic Shell Intrinsics
- **`activities`**: Lists all active processes and process groups spawned by the shell, showing PIDs, PGIDs, and states (`Running` or `Stopped`).
- **Terminal Control**: Robust job control handling `SIGINT` (Ctrl-C), `SIGTSTP` (Ctrl-Z), and `SIGTTOU` via `tcsetpgrp()`.
- **`resume`**: Resumes stopped or backgrounded jobs into foreground (`fg`) or background (`bg`). Supports execution time limits with `--timeout <seconds>`.
- **`ping`**: Delivers signals to processes by PID or process group by `%job_number`.

#### Part F: System Diagnostics
- **`spy`**: Inspects open file descriptors, types (CWD, TXT, MEM, REG, CHR), and file paths for any process via the `/proc` filesystem.
- **`snoop`**: Traces system calls made by a command or running process using `ptrace()`, reporting call counts and execution time statistics.

### Building & Running C-Shell

To compile and launch the C-Shell:
```bash
cd c-shell
make all
./shell.out
```

To clean compiled object files and binaries:
```bash
make clean
```

---

## ⚙️ Part 2: xv6 MLFQ Scheduler

The default xv6 operating system scheduler uses a Round-Robin (RR) policy. In this project, an additional **Multi-Level Feedback Queue (MLFQ)** scheduler was implemented inside the RISC-V kernel.

### Key MLFQ Features
- **4 Priority Queues**: Priority levels 0 (highest) to 3 (lowest).
- **Time Quanta**:
  - Queue 0: 1 tick
  - Queue 1: 4 ticks
  - Queue 2: 8 ticks
  - Queue 3: 16 ticks (Round-Robin)
- **Strict Priority Selection**: The scheduler always executes runnable processes from the highest non-empty queue.
- **Time-Slice Exhaustion & Demotion**: Processes consuming their full time slice in a queue are demoted to the next lower queue.
- **Voluntary Yielding**: Processes yielding CPU before slice exhaustion (e.g. for I/O) remain in their current priority queue.
- **Priority Boosting**: Every **48 ticks**, all processes are boosted back to Queue 0 to prevent starvation.
- **Debugging & Metrics**: Enhanced `procdump()` (`Ctrl+P`) and custom `schedulertest` benchmark tool.

### Building & Running xv6

To build and run xv6 with the **default Round-Robin scheduler**:
```bash
cd xv6
make clean
make qemu
```

To build and run xv6 with the **MLFQ scheduler**:
```bash
cd xv6
make clean
make qemu SCHEDULER=MLFQ
```

---

## 📊 Evaluation & Report

Detailed design explanations, MLFQ timeline plots, and empirical benchmarking results comparing **FIFO**, **Round-Robin**, and **MLFQ** are documented in [`xv6/report.md`](file:///Users/shubhamgupta/Desktop/mp1/xv6/report.md).

---

## 🤖 AI Usage

In accordance with project guidelines, AI tool interactions and prompt history are fully documented in [`AI-usage.md`](file:///Users/shubhamgupta/Desktop/mp1/AI-usage.md) / `AI-usage.pdf`.

**AI Share Session Links**:
1. [Claude Share 1](https://claude.ai/share/aa18c456-91cc-458a-b666-87d8f74a285d)
2. [Claude Share 2](https://claude.ai/share/a7caebcb-ea9d-4ca5-ae91-39380776eb00)
3. [Claude Share 3](https://claude.ai/share/bb70ae08-b6d7-4b6a-9168-b96c8a198453)
4. [Claude Share 4](https://claude.ai/share/231b8a2b-70d8-4b85-b26e-fddbdde662cf)
5. [Claude Share 5](https://claude.ai/share/9d84c810-3193-4a64-920e-4d9d9ec7a645)
6. [Claude Share 6](https://claude.ai/share/5e50bb2f-9435-4ab0-86c1-fe8b27976c76)
7. [Claude Share 7](https://claude.ai/share/ea342d9e-b48d-4c44-b633-c6664c878e35)
8. [Claude Share 8](https://claude.ai/share/ca76ab64-fe42-4f48-a5aa-02c12d4c315d)
9. [ChatGPT Share](https://chatgpt.com/share/6a8dc654-a6e0-83ee-b17b-b5f88a7dd671)
10. [ChatGPT Custom GPT Project](https://chatgpt.com/g/g-p-6a871d2222b08191b0e12393ef92109d-osn-mp1/project)



