<div align="right">
    <a href="./README.md">📖 EN</a>
</div>

# libbpf-bootstrap: BPF demo

针对官方libbpf-bootstrap的一些demo，进行了一些修改

## test

`test` 项目当你出发内核`do_unlinkat`方法的时候打印一些数据(当你rm文件的时候触发)

```shell
$ cd examples/c
$ make test
$ sudo ./test
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
           <...>-3840345 [010] d... 3220701.101143: bpf_trace_printk: BPF triggered from PID 3840345.
           <...>-3840345 [010] d... 3220702.101265: bpf_trace_printk: BPF triggered from PID 3840345.
```



## uprobe_go

`uprobe_go` 是可以插桩用户态代码的例子，首先你需要运行`go run fetch.go`的go程序，然后加载`unprobe_go`的bpf模块，之后就可以在trace_pipe中看到抓到的内容。

```shell
$ sudo ./uprobe_go
libbpf: loading object 'uprobe_bpf' from buffer
...
Successfully started!
...........
```

在`/sys/kernel/debug/tracing/trace_pipe`中查看:
```shell
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
        fetch-161527  [000] d..21 4059879.754534: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
		fetch-159539  [000] d..21 4059884.784151: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
		fetch-161527  [001] d..21 4059889.816504: bpf_trace_printk: UPROBE fetch pid = 159539, code_name = sh601006GoStrin
```



## xdp

`xdp` 在这里的例子和`tc_trace`一样，也是打印更多数据，第一个参数如果不指定，默认就是回环网卡`lo`。这里如果不需要对skb有更多的操作，建议使用xdp。

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

`tc_trace` 这里的例子就是在网卡接受数据的时候，在封装好skb_buf之后，插桩拿到的数据，`INGRESS`主要是针对接受数据进行插桩；`EGRESS`是针对发送数据进行插桩。第一个参数如果不指定，默认就是回环网卡`lo`。如果需要对发送数据进行过滤或者处理，建议用tc，因为xdp拿不到发送数据。

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


## tc_egress



## lsm_control
`lsm_control` 这里的例子，主要针对`file_open()`的系统函数进行拦截，如果发现正在操作`/etc/passwd`直接禁止，这个bpf主要可以作为一些系统安全阀门使用。

```shell
$ sudo ./lsm_control
libbpf: loading object 'lsm_control_bpf' from buffer
...
Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` to see output of the BPF programs.
..........
```

输出在`/sys/kernel/debug/tracing/trace_pipe`可以看到:

````shell
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
         /home/sunder/bin/stock# cat /sys/kernel/debug/tracing/trace_pipe |grep passwd
           <...>-2728    [000] d..21 4061889.067397: bpf_trace_printk: LSM_TEST: open file passwd
            bash-2728    [000] d..21 4061891.042296: bpf_trace_printk: LSM_TEST: open file passwd
             cat-169293  [000] d..21 4061891.692616: bpf_trace_printk: LSM_TEST: open file passwd
              vi-169812  [001] d..21 4062035.939314: bpf_trace_printk: LSM_TEST: open file passwd
              vi-169812  [001] d..21 4062035.939669: bpf_trace_printk: LSM_TEST: open file passwd
````

当试图操作`/etc/passwd`的时候，内核会调用`file_open()`函数，这时候会被我们的bpf例子拦截，并提示错误：

```shell
$ cat /etc/passwd
cat: /etc/passwd: Operation not permitted
```



# 编译

以上例子是基于libbpf-bootstrap进行开发，在编译前，把同目录的代码复制到libbpf-bootstrap/example/c下面，然后修改Makefile，把test，uprobe_go，xdp，tc_trace，lsm_control，tc_egress加到`APPS =`这行后面，然后make编译即可。