# TCPer: Reliable & High-Speed File Transmission

**TCPer** is a lite, high-performance file transmission tool written in C language. It leverages the speed of TCP protocol while implementing a reliability layer to ensure your data arrives intact and fast.

> "Transmitting data at the speed of UDP, with the reliability you expect from TCP."

## Features

- **Reliable**: Implements custom mechanisms to handle packet loss and ordering, ensuring 100% data integrity.
- **High Performance**: Optimized with `O2` level compilation and Link Time Optimization (`-flto`) for maximum throughout.
- **Integrity Verification**: Built-in MD5 checksumming to verify files post-transmission.
- **Multi-Thread**: Powered by `POSIX` or `WIN32API` threads for efficient `I/O` and data processing.
- **Simple CLI**: Minimalist command-line interface with a cool ASCII art identity.
- **Cross-Platform**: Modular design with abstraction layers for OS, Sockets, and Threads in Windows, Linux and macOS.

## Quick Start

### Build from Source

Ensure you have `gcc` and `make` installed on your system environment path.

```zsh
>>> gcc --version

Apple clang version 15.0.0 (clang-1500.3.9.4)
Target: arm64-apple-darwin23.6.0
Thread model: posix
InstalledDir: /Library/Developer/CommandLineTools/usr/bin
```

```zsh
>>> make --version

GNU Make 3.81
Copyright (C) 2006 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

This program built for i386-apple-darwin11.3.0
```

### Usage

**Step 1: Clone the GitHub repository**

```bash
>>> git clone https://github.com/Illusionna/TCPer.git
>>> cd TCPer
```

**Step 2: Compile the project with C language `make` in multi-threads**

```bash
>>> make -j4
```

**Step 3: Configure the server parameter in client**

```bash
>>> ./tcp config 192.168.1.100:8080
```

**Step 4: Start the server (`Receiver` which runs this on the machine that will receive the file)**

```bash
>>> ./tcp run
```

**Step 5: Send a file from client to server (`Sender` which transmits a file to `Receiver`)**

```bash
>>> ./tcp send ./Desktop/Illusionna/demo.pdf
```
