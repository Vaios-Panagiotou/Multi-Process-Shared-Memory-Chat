# Multi-Process Shared-Memory Chat (C)

Course assignment project implementing a local chat system in C with:

- Multiple processes (one process per terminal user)
- One receiver thread per process
- POSIX shared memory for global chat state
- Inter-process synchronization with semaphores and condition variables

The application is terminal-based and supports creating/joining/leaving dialogs and exchanging messages between participants.

## Author

- Vaios Panagiotou

## Repository Structure

- `main.c`: command loop, startup/shutdown flow, receiver thread creation
- `shared.h` / `shared.c`: shared-memory data model, initialization, synchronization utilities, dialog/message core logic
- `dialog.h` / `dialog.c`: dialog-level wrapper functions (`create`, `join`, `leave`)
- `message.h` / `message.c`: message-level wrapper functions (`send`, `collect`)
- `receiver.h` / `receiver.c`: background receiver thread that prints incoming messages
- `Makefile`: build and cleanup targets

## Core Architecture

### 1. Shared Memory Region

All processes map the same shared memory object (`SHM_NAME`) containing:

- Dialog table (`dialogs`)
- Message table (`msgs`)
- ID counters (`next_dialog_id`, `next_msg_id`)
- Process-shared semaphore (`mutex`) for critical sections
- Process-shared mutex + condition variable (`cv_mutex`, `cv_newmsg`) for receiver wake-up

Key limits are configurable in `shared.h`:

- `MAX_DIALOGS`
- `MAX_PARTICIPANTS`
- `MAX_MSGS`
- `MAX_PAYLOAD`

### 2. Process + Thread Model

Each terminal instance runs:

- Main thread: reads user commands and updates shared state
- Receiver thread: waits for new messages and prints them asynchronously

This enables interactive input and asynchronous message display at the same time.

### 3. Synchronization Strategy

- `sem_wait/sem_post` on `shm->mutex` protect all shared data mutations
- `pthread_cond_broadcast` on `cv_newmsg` wakes waiting receiver threads after updates
- Receiver thread avoids busy waiting by sleeping on `pthread_cond_wait`

Important rule followed in the implementation:

- broadcast notifications happen after releasing shared-memory critical sections

### 4. Message Delivery Semantics

Each message stores `remaining_reads` (how many recipients still need to read it).

- Sender does not consume their own message
- Each receiver decrements `remaining_reads`
- When `remaining_reads == 0`, the message slot is marked free (`used = 0`)

This gives one-delivery-per-recipient behavior.

## Supported Commands

- `create`
	- Creates a new dialog
	- Adds current process as participant
- `join <dialog_id>`
	- Joins an existing active dialog
- `leave <dialog_id>`
	- Leaves a dialog
- `send <dialog_id> <text>`
	- Sends a message to members of the selected dialog
- `send <dialog_id> TERMINATE`
	- Sends termination message and triggers orderly exit flow
- `list`
	- Prints active dialogs and message state snapshot
- `exit`
	- Leaves current dialog (if any) and exits process

## Build

Requirements:

- GCC
- POSIX threads
- POSIX realtime library (`-lrt`)
- `make`

Build the executable:

```bash
make
```

Clean artifacts:

```bash
make clean
```

## Run

Open 2+ terminals in the project folder and run:

```bash
./chat
```

Example flow:

Terminal A:

```text
create
```

Terminal B:

```text
join 1
send 1 hello from B
```

Terminal A should asynchronously print the incoming message.

## Error Handling and Safety Notes

- Join/create guards prevent a PID from being in more than one dialog at once
- Dead processes are periodically removed from participant lists
- Shared memory is unlinked when no active dialogs remain
- Input parsing for `send` checks argument format (`send <id> <text>`)

## Known Limitations

- Local machine IPC only (not networked)
- Fixed-size tables (capacity limits from compile-time constants)
- Minimal command parser (no quoting/escaping support)
- Output can still be visually noisy under heavy concurrent activity
