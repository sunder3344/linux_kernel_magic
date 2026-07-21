#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

#define EPERM  1

static __always_inline bool str_equal(const char *a, const char *b, int len) {
#pragma unroll
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

//根据lsm_hook_defs.h里面的hooks找
SEC("lsm/file_open")
int BPF_PROG(lsm_file_open, struct file *file, int ret)
{
    /* ret is the return value from the previous BPF program
     * or 0 if it's the first hook.
     */
    if (ret)
        return ret;

    char path[128];
    const unsigned char *name;
    struct dentry *dentry;
    dentry = BPF_CORE_READ(file, f_path.dentry);
    name = BPF_CORE_READ(dentry, d_name.name);
    bpf_probe_read_kernel_str(path, sizeof(path), name);

    bpf_printk("LSM_TEST: open file %s\n", path);
    if (str_equal(path, "passwd", 6) == 1) {
    	bpf_printk("LSM_TEST: block file");
    	return -EPERM;
    }
    return 0;
}
