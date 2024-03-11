# bpftrace

`bpftrace` is a high-level tracing language and runtime for Linux based on BPF.

It can be used for tracing and debugging applications without the need of recompilation.

It can be installed with this command:

```
sudo apt install bpftrace
```

## Examples

### malloc

Here's a bpftrace script that collects hived's data about requested memory allocation sizes when calling malloc and prints a histogram every 2s:

```
#!/usr/bin/env bpftrace

uprobe:/proc/9401/root/lib/x86_64-linux-gnu/libc.so.6:malloc /comm == "hived"/ {
  @allocations = hist(arg0);
}

interval:s:2 {
  print(@allocations);
}
```

Wer'e running `hived` in a docker container,  so we can't use `/lib/x86_64-linux-gnu/libc.so.6`, because that's libc of the host system. We need to use libc in the container and we can abuse `/proc` to get it. But we need PID of running container for this to work (in the example above it's 9401). We can get the PID with this command:

```
docker inspect CONTAINER_NAME | grep -iw Pid
```

An example output of the script above:

```
@allocations:
[1]                 2268 |                                                    |
[2, 4)              1830 |                                                    |
[4, 8)             37548 |                                                    |
[8, 16)           148702 |@@                                                  |
[16, 32)         3062265 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[32, 64)         1688478 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@                        |
[64, 128)         671633 |@@@@@@@@@@@                                         |
[128, 256)        373235 |@@@@@@                                              |
[256, 512)         88852 |@                                                   |
[512, 1K)          93952 |@                                                   |
[1K, 2K)           26338 |                                                    |
[2K, 4K)           19171 |                                                    |
[4K, 8K)            5303 |                                                    |
[8K, 16K)            352 |                                                    |
[16K, 32K)          1224 |                                                    |
[32K, 64K)          1639 |                                                    |
[64K, 128K)         1424 |                                                    |
[128K, 256K)          90 |                                                    |
[256K, 512K)           2 |                                                    |
[512K, 1M)            90 |                                                    |
[1M, 2M)               2 |                                                    |
[2M, 4M)             512 |                                                    |
[4M, 8M)               0 |                                                    |
[8M, 16M)           3774 |                                                    |
```

### Bytes read

Here's another script that draws histogram of bytes read by `hived`.

```
#!/usr/bin/env bpftrace

tracepoint:syscalls:sys_exit_read /comm == "hived" / {
    @bytes = hist(args->ret);
}

interval:s:2 {
    print(@bytes);
}
```

This attaches to a syscall, so we don't need PID of a container.

### Futexes

And here's a script that draws a histogram of time `hived` spends in futexes:

```
#!/usr/bin/env bpftrace

tracepoint:syscalls:sys_enter_futex /comm == "hived"/ {
    @start[tid] = nsecs;
}

tracepoint:syscalls:sys_exit_futex /comm == "hived"/ {
    @end[tid] = nsecs;
    @duration = hist(nsecs - @start[tid]);
    delete(@start[tid]);
}

interval:s:5 {
    print(@duration);
}
```

### Attaching to a custom function

`bpftrace` can also attach to any publically visible function in an executable. Here's a script drawing histogram of times spent in `database::apply_block`:

```
uprobe:/home/haf_admin/workspace/hive/build/programs/hived/hived:_ZN4hive5chain8database11apply_blockERKSt10shared_ptrINS0_15full_block_typeEEjPKNS0_18block_flow_controlE {
  @start[tid] = nsecs;
}

uretprobe:/home/haf_admin/workspace/hive/build/programs/hived/hived:_ZN4hive5chain8database11apply_blockERKSt10shared_ptrINS0_15full_block_typeEEjPKNS0_18block_flow_controlE /@start[tid]/ {
  @apply_block_times = hist(nsecs - @start[tid]);
  delete(@start[tid]);
}

interval:s:2 {
    print(@apply_block_times);
}
```

The function name must be mangled. This can be obtained using `gdb`:

`gdb ./programs/hived/hived` and then `disas hive::chain::database::apply_block` in gdb prompt.

Newer `bpftrace` seems to support automatic mangling, but Ubuntu version is too old.

## More

Further help and more examples can be found here:

- https://manpages.ubuntu.com/manpages/focal/en/man8/bpftrace.bt.8.html
- https://man.archlinux.org/man/bpftrace.8.en
