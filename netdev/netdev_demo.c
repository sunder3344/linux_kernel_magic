#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#define VNET_NAME "virtual_net0"
#define VNET_WEIGHT 64			//NAPI轮询一次最多处理64个数据包

static struct net_device *vnet_dev;
static struct napi_struct vnet_napi;

//发送数据，透传到eth0
static netdev_tx_t demo_xmit(struct sk_buff *skb, struct net_device *dev) {
	struct net_device *real_dev = dev_get_by_name(&init_net, "enp0s3");		//绑定物理卡
	if (!real_dev) {
		pr_err("eth0 not found!\n");
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	skb->dev = real_dev;
	dev_queue_xmit(skb);		//让eth0发送数据
	pr_info("virtual_net0: Packet sent to eth0\n");
	return NETDEV_TX_OK;
}

//处理NAPI轮询
static int demo_napi_poll(struct napi_struct *napi, int budget) {
	int processed = 0;
	
	while (processed < budget) {
		struct sk_buff *skb;
		struct ethhdr *eth;
		
		//分配一个sk_buff
		skb = dev_alloc_skb(1500);
		if (!skb)
			break;

		skb_put(skb, 64);		//设置包大小
		skb->dev = vnet_dev;
		skb->protocol = htons(ETH_P_IP);
		skb_reset_mac_header(skb);

		//设置mac头部
		eth = eth_hdr(skb);
		memset(eth->h_dest, 0xff, ETH_ALEN);		//目标 MAC 地址 (广播)
		memset(eth->h_source, 0x00, ETH_ALEN);		//源 MAC 地址
		eth->h_proto = htons(ETH_P_IP);

		/*传递数据包到协议栈*/
		netif_receive_skb(skb);
		processed++;
	}

	if (processed < budget) {
		napi_complete_done(napi, processed);
	}
	
	return processed;
}

//触发NAPI轮询（模拟硬件中断）
static void demo_napi_trigger(struct net_device *dev) {
	napi_schedule(&vnet_napi);
}

//设备打开
static int demo_open(struct net_device *dev) {
	netif_start_queue(dev);
	napi_enable(&vnet_napi);
	pr_info("virtual_net0: Device opening\n");
	
	//模拟设备收到数据，触发NAPI轮询
	demo_napi_trigger(dev);
	return 0;
}

//设备关闭
static int demo_stop(struct net_device *dev) {
	netif_stop_queue(dev);
	napi_disable(&vnet_napi);
	pr_info("virtual_net0: Device closed\n");
	return 0;
}

//setup net_device_ops
static const struct net_device_ops demo_netdev_ops = {
	.ndo_open = demo_open,
	.ndo_stop = demo_stop,
	.ndo_start_xmit = demo_xmit,			//发送数据
	.ndo_poll_controller = demo_napi_trigger,		//触发NAPI沦陷
};

static int __init vnet_init(void) {
	//申请net_device
	vnet_dev = alloc_etherdev(0);
	if (!vnet_dev) {
		pr_err("Failed to allocate net_device\n");
		return -ENOMEM;
	}

	//setup device name
	strcpy(vnet_dev->name, VNET_NAME);
	//bind net_device_ops
	vnet_dev->netdev_ops = &demo_netdev_ops;
	//init NAPI
	netif_napi_add(vnet_dev, &vnet_napi, demo_napi_poll);
	
	//register NIC
	if (register_netdev(vnet_dev)) {
		pr_err("Failed to register net_device\n");
		free_netdev(vnet_dev);
		return -ENODEV;
	}

	pr_info("virtual_net0: Virtual net device registered\n");
	return 0;
}

static void __exit vnet_exit(void) {
	unregister_netdev(vnet_dev);
	netif_napi_del(&vnet_napi);
	free_netdev(vnet_dev);
	pr_info("virtual_net0: Virtual net device unregistered\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek-sunder");
MODULE_DESCRIPTION("virtual network device with NAPI");
