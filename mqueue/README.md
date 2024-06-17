# linux mqueue

posix消息队列

编译：
gcc -o mqueue_receive mqueue_receive.c -lrt
gcc -o mqueue_sender mqueue_sender.c -lrt

./mqueue_receive
./mqueue_sender

在/dev/mqueue/目录下会创建一个消息队列，上面例子用的mq_receive同步的方式接受消息，可以使用mq_notify来异步接受消息。