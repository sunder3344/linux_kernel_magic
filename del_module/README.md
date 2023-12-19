# rmmod problem（linux 3.10）

当使用rmmod移除linux内核模块时，系统报"rmmod: ERROR: Module XXX is in use"

1. make编译diamante
2. lsmod查看该模块计数有几个占用
3.  insmod force_rmmod.ko modname=*** m_incs=0 m_decs=1 将m_decs后面的数填成该模块计数占用的数即可
4. rmmod *** 直接移除