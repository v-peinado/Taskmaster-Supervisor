# Taskmaster

A job control daemon written in C++20, similar to [supervisor](http://supervisord.org/).

It launches programs described in a YAML config file, keeps them alive according to
their restart policy, and provides a control shell to inspect and manage them while
running. The configuration can be reloaded on the fly without restarting the daemon
or the programs that have not changed.

## Requirements

- A C++20 compiler
- Linux kernel 5.3 or newer
- `libyaml-cpp-dev`, used only to parse the configuration file

```bash
sudo apt install libyaml-cpp-dev
```

Taskmaster uses `pidfd_open` to detect when a supervised process dies, and that
syscall needs kernel 5.3 or newer. On older systems the daemon builds and runs,
but it cannot notice that its children have died, so they pile up as zombies and
are never restarted. You can check your kernel with `uname -r`, and if it is too
old the simplest workaround is to run taskmaster inside a virtual machine or a
container with a recent kernel.

Old glibc headers are not a problem: if `P_PIDFD` or `SYS_pidfd_open` are missing,
they are defined by hand and the syscall is called directly.
## Build

```bash
make
```

## Usage

```bash
./taskmaster <config file>
```

Once running, the control shell accepts:

| Command | Description |
|---|---|
| `status` | show every program and its state |
| `start <program>` | start a program |
| `stop <program>` | stop a program |
| `restart <program>` | restart a program |
| `reload` | reload the configuration file |
| `help` | show the available commands |
| `quit` | stop everything and exit |

The configuration can also be reloaded from outside by sending a `SIGHUP`:

```bash
kill -HUP $(pgrep -f "^./taskmaster")
```

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

Only `cmd` is mandatory; every other field falls back to its default.

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

Each instance is named `<program>_<index>`, so `nginx` with `numprocs: 2` runs as
`nginx_0` and `nginx_1`.

Events are logged to `./logs/app.log`.