<div align="right">
    <a href="./README_CN.md">📖 中文</a>
</div>

# libbpf-bootstrap: BPF demo

Some BPF demo for detail application

## test

`test` is just that – a minimal practical BPF application example. It print more info when 
you trigger `do_unlinkat` function.

```shell
$ cd examples/c
$ make test
$ sudo ./test
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
           <...>-3840345 [010] d... 3220701.101143: bpf_trace_printk: BPF triggered from PID 3840345.
           <...>-3840345 [010] d... 3220702.101265: bpf_trace_printk: BPF triggered from PID 3840345.
```



## uprobe_go

`uprobe_go` is an example of dealing with user-space entry and exit (return) probes.
First you should run golang program `fetch.go`, and then load `uprobe.bpf`. The user-space
function is triggered once every second:

```shell
$ sudo ./uprobe_go
libbpf: loading object 'uprobe_bpf' from buffer
...
Successfully started!
...........
```

You can see `uprobe` demo output in `/sys/kernel/debug/tracing/trace_pipe`:
```shell
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
        fetch-161527  [000] d..21 4059879.754534: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
		fetch-159539  [000] d..21 4059884.784151: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
		fetch-161527  [001] d..21 4059889.816504: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
```



## xdp

`xdp` in this demo is similar with `tc_trace`, it attempts to print more info(IP, port, Payload). The first parameter is NIC name, if you leave a blank, the default value is `lo`.

```shell
$ sudo ./target/release/xdp eth0
..........
================ [ 捕获到 HTTP 报文 ] ================
来源: 169.254.0.203:80 -> 目的: 169.254.0.203:52346
---- 内容明文 (前128字节) ----
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Date: Tue, 21 Jul 2026 07:37:25 GMT
Content-Length: 74
Connection: close

{"msg":"OK","returnCode":0,"returnValue":0,"seqId":"d9fi3dd220evefecv6f0"}
```



## tc_trace

`tc_trace` (short for Traffic Control) is an example of handling ingress network traffics.
In this situation, the demo set `INGRESS` and catch more infomation(IP, port, payload). The first parameter is NIC name, if you leave a blank, the default value is `lo`.

```shell
$ sudo ./tc_trace eth0
...
================ [ 捕获到 HTTP 报文 ] ================
来源: 169.254.0.203:80 -> 目的: 169.254.0.203:52346
---- 内容明文 (前128字节) ----
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Date: Tue, 21 Jul 2026 07:37:25 GMT
Content-Length: 74
Connection: close

{"msg":"OK","returnCode":0,"returnValue":0,"seqId":"d9fi3dd220evefecv6f0"}
......
```



## kprobe_sendmsg

`kprobe_sendmsg` can catch the payload before you send msg through socket. In this example, the `tcp_sendmsg()` kernel function will trigger the printing.

```shell
$ sudo ./kprobe_sendmsg
libbpf: loading object 'kprobe_sendmsg' from buffer
...
Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` to see output of the BPF programs.
..........
```

The output from `lsm_control` in `/sys/kernel/debug/tracing/trace_pipe` is expected to resemble the following:

````shell
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
           <...>-944575  [001] d..31 4237610.143110: bpf_trace_printk: pid 944575 send payload: POST / HTTP/1.1
			Host: www.baidu.com
			User-Agent: curl/7.88.1
			Accept: */*
			Content-Length: 20
			Content-Type: application/x-www-form-urlencoded
			
			this is sendmsg demo
````



## lsm_control
`lsm_control` serves as an illustrative example of utilizing [LSM BPF](https://docs.kernel.org/bpf/prog_lsm.html). In this example, the `file_open()` system call(when you attempt to open `/etc/passwd`) is effectively blocked.

```shell
$ sudo ./lsm_control
libbpf: loading object 'lsm_control_bpf' from buffer
...
Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` to see output of the BPF programs.
..........
```

The output from `lsm_control` in `/sys/kernel/debug/tracing/trace_pipe` is expected to resemble the following:

````shell
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
         /home/sunder/bin/stock# cat /sys/kernel/debug/tracing/trace_pipe |grep passwd
           <...>-2728    [000] d..21 4061889.067397: bpf_trace_printk: LSM_TEST: open file passwd
            bash-2728    [000] d..21 4061891.042296: bpf_trace_printk: LSM_TEST: open file passwd
             cat-169293  [000] d..21 4061891.692616: bpf_trace_printk: LSM_TEST: open file passwd
              vi-169812  [001] d..21 4062035.939314: bpf_trace_printk: LSM_TEST: open file passwd
              vi-169812  [001] d..21 4062035.939669: bpf_trace_printk: LSM_TEST: open file passwd
````

When the `file_open()` system call(attemp to open `/etc/passwd`) gets blocked, the `cat /etc/passwd` command yields the following output:

```shell
$ cat /etc/passwd
cat: /etc/passwd: Operation not permitted
```



# Building

libbpf-bootstrap supports multiple build systems that do the same thing.
This serves as a cross reference for folks coming from different backgrounds.