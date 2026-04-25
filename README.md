# Unix-Shell-C

A Unix-like shell implemented in C featuring built-in commands, pipes, background execution, process management, and integrated TCP networking support.

---

## Table of Contents

* Project Overview
* Technical Implementation
* Built-in Commands
* Integrated Networking System
* Building and Running
* Basic Commands
* Networking Commands


## Project Overview

This project implements a Unix-like shell in C called `mysh`. The shell provides an interactive command-line interface that supports built-in commands, process management, pipes, background execution, and networking functionality through integrated server and client commands.
The goal of the project was to recreate core Linux shell behavior using low-level POSIX system calls such as `fork()`, `exec()`, `pipe()`, `dup2()`, `signal()`, `waitpid()`, `socket()`, `bind()`, `listen()`, `accept()`, and `select()`.

---

## Technical Implementation

The shell is designed around core systems programming concepts, including input parsing, command execution, process control, and networking.
User input is read through the interactive prompt (`mysh$`) and tokenized to determine whether the command should be handled as a builtin, executed using `/bin` or `/usr/bin`, or processed as a pipe or background task.
The shell supports variable expansion and command parsing before execution, allowing commands to be dynamically resolved while maintaining safe memory handling and token limits. Assignment statements, expanded commands, and builtin detection are handled before execution begins.
Process execution is managed using `fork()`, `exec()`, `waitpid()`, and `dup2()`. Builtins that must affect shell state, such as `cd`, `start-server`, and `close-server`, run directly inside the shell process, while others execute in child processes. Pipe detection supports chaining commands using `|`, and background execution using `&` allows long-running processes to continue without blocking the shell.
Special care was taken with signal handling, particularly `SIGINT`. Pressing `Ctrl+C` never terminates the shell itself, but foreground child processes and builtins should still terminate normally. This mirrors standard Unix shell behavior while preserving shell stability.

---

## Built-in Commands

The shell supports the following built-in commands:

* `echo`
* `cd`
* `ls`
* `cat`
* `wc`
* `ps`
* `kill`
* `exit`
* `start-server`
* `close-server`
* `send`
* `start-client`

Commands not implemented internally are executed using `/bin` or `/usr/bin` through `fork()` and `exec()`.

---

## Integrated Networking System

One of the most advanced features of the shell is the built-in TCP multi-client chat system.

Networking features include:

* concurrent client connections
* message broadcasting to all connected clients
* assigned client IDs (`client1:`, `client2:`)
* `\connected` command to display active clients
* message length validation (128-character limit)
* proper cleanup of client/server processes

The networking system is implemented using POSIX sockets and `select()` for multiplexed I/O, allowing the server to remain non-blocking while supporting multiple simultaneous clients.

---

## Building and Running

Compile the shell:

```bash
make
```

Remove object files and executable:

```bash
make clean
```

Run the shell:

```bash
./mysh
```

Prompt:

```bash
mysh$
```

---

## Basic Commands

### Change Directory

```bash
mysh$ cd ..
mysh$ cd ../../folder
mysh$ cd
```

Supports:

* relative paths
* absolute paths
* `.`
* `..`
* arbitrary dot expansion (`...`, `....`, etc.)

---

### Directory Listing

```bash
mysh$ ls
mysh$ ls --a
mysh$ ls --rec --d 2
mysh$ ls --f txt
```

Supports:

* hidden files
* recursive traversal
* search depth control
* substring filtering

---

### File Contents

```bash
mysh$ cat notes.txt
```

Displays file contents to standard output.

---

### Word Count

```bash
mysh$ wc notes.txt
```

Example output:

```text
word count 251
character count 1234
newline count 25
```

Implemented manually without string methods.

---

### Process Management

```bash
mysh$ sleep 30 &
mysh$ ps
mysh$ kill 4821
```

Supports:

* background execution
* active process tracking
* signal sending
* completion reporting

---

## Networking Commands

### Start Server

```bash
mysh$ start-server 9000
```

Starts a non-blocking background chat server.

---

### Start Client

Open another terminal:

```bash
mysh$ start-client 9000 127.0.0.1
```

The client stays connected and can continue sending messages.

---

### Check Connected Clients

From the client:

```bash
\connected
```

Example output:

```text
1
```

Displays the number of currently connected clients.

---

### Send One-Time Message

```bash
mysh$ send 9000 127.0.0.1 hello from the server
```

This broadcasts the message to:

* the server terminal
* all connected clients

---

### Close Server

```bash
mysh$ close-server
```

Terminates the active server and cleans up remaining processes.

---

