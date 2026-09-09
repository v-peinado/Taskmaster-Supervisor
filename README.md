# Taskmaster

A job control daemon written in C++20, similar to [supervisor](http://supervisord.org/).

It launches the programs described in a YAML config file, keeps them alive according to their
restart policy, and offers a control shell to manage them while running. The configuration can be
reloaded on the fly without restarting the daemon or the programs that have not changed.

Two binaries: `taskmasterd`, the daemon that does the job control, and `taskmasterctl`, the client
that talks to it over a UNIX socket.

## Requirements

- A C++20 compiler
- Linux kernel 5.3 or newer (needed for `pidfd_open`, check with `uname -r`)
- `libyaml-cpp-dev` for the server, `libreadline-dev` for the client

```bash
sudo apt install libyaml-cpp-dev libreadline-dev
```

## Build

```bash
make
```

## Usage

Start the daemon with a config file. It detaches from the terminal and returns immediately:

```bash
./taskmasterd conf.yml
```

Then talk to it with the client:

```bash
./taskmasterctl
```

| Command | Description |
|---|---|
| `status` | show every program and its state |
| `start <program>` | start a program |
| `stop <program>` | stop a program |
| `restart <program>` | restart a program |
| `reload` | reload the configuration file |
| `help` | show the available commands |
| `shutdown` | stop the daemon and all its programs |
| `exit` | leave the client, the daemon keeps running |

Tab completes command names and the arrow keys walk through the history. The config can also be
reloaded from outside with `kill -HUP $(pgrep -f taskmasterd)`.

## Configuration

```yaml
programs:

  nginx:
    cmd: "/usr/local/bin/nginx -c /etc/nginx/test.conf"
    numprocs: 1
    umask: 022
    workingdir: /tmp
    autostart: true
    autorestart: unexpected
    exitcodes:
      - 0
      - 2
    startretries: 3
    starttime: 5
    stopsignal: TERM
    stoptime: 10
    stdout: /tmp/nginx.stdout
    stderr: /tmp/nginx.stderr
    env:
      STARTED_BY: taskmaster
      ANSWER: "42"
```

Only `cmd` is mandatory; everything else falls back to its default.

| Field | Default | Description |
|---|---|---|
| `cmd` | — | command used to launch the program |
| `numprocs` | 1 | how many instances to run |
| `umask` | inherited | umask applied before launching (octal) |
| `workingdir` | inherited | directory to move into before launching |
| `autostart` | true | start it when taskmaster starts |
| `autorestart` | unexpected | `always`, `never` or `unexpected` |
| `exitcodes` | 0 | exit codes considered a normal termination |
| `startretries` | 3 | how many times to retry a failed start |
| `starttime` | 1 | seconds it must stay alive to count as started |
| `stopsignal` | TERM | signal used to stop it gracefully |
| `stoptime` | 10 | seconds to wait before sending SIGKILL |
| `stdout` | discarded | file to write its standard output to |
| `stderr` | discarded | file to write its standard error to |
| `env` | inherited | environment variables added before launching |

Instances are named `<program>_<index>`, so `nginx` with `numprocs: 2` runs as `nginx_0` and
`nginx_1`. The index is always added, even for a single instance.

A program that dies before its `starttime` counts as a failed start and is retried up to
`startretries` times, whatever its exit code. One-shot commands therefore need `starttime: 0`.

## Logs

Daemon events go to `./logs/app.log`, rotated by size and cleaned by age, and also to syslog:

```bash
journalctl -t taskmaster -f
```

Each supervised program writes to its own files, set by `stdout` and `stderr`.

## Documentation

`docs/` holds the development log and further notes on the design.