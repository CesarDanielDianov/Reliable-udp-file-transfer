# Reliable UDP File Transfer

File transfer in C that delivers files reliably over **UDP** using a **sliding-window ARQ** protocol with selective acknowledgments, timeouts and fast retransmit.

The same implementation runs as any of the three classic ARQ algorithms, depending on the window sizes:

| Mode | Sender window | Receiver window |
|---|---|---|
| Stop-and-Wait | 1 | 1 |
| Go-Back-N | N > 1 | 1 |
| Selective Repeat | N > 1 | M > 1 |

> Project 1 for *Computer Networks* (2026/27) at Instituto Superior Técnico. The skeleton, `packet-format.h` and the debugging tools were provided by the course staff; the ARQ logic in `file-sender.c` and `file-receiver.c` is my own.

## How it works

- Files are split into 1000-byte chunks; the last chunk is shorter (possibly empty).
- Each ACK carries the receiver's **base** (first missing chunk) and a **bit mask** of the chunks already received after it.
- **Receiver:** stores out-of-order chunks inside its window, slides the base over consecutive received chunks and acknowledges every datagram. It exits 4 s after the last packet, deleting the file if incomplete.
- **Sender:** keeps up to N chunks in flight. A timeout (1 s) retransmits only the chunks not yet acknowledged, and 3 duplicate ACKs trigger a fast retransmit of the base. It gives up after 3 consecutive timeouts.

## Build and run

```bash
make
./file-receiver <output-file> <port> <receive-window>
./file-sender   <input-file> <host> <port> <send-window>
```

Example:

```bash
./file-receiver out.dat 1234 4
./file-sender   in.dat localhost 1234 4
```

## Testing

`run.sh` runs a full scenario with simulated packet loss (via `log-packets.so` and `LD_PRELOAD`) and draws a message sequence chart with `generate-msc.sh` (requires `mscgen`):

```bash
bash run.sh
evince msc.eps
```
