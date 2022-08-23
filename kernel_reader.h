
/* Note: run this program as root user
 * Author:Subodh Saxena
 */

//! code source is from https://onedrive.live.com/?authkey=%21AC8SHTh48t67WcY&cid=79CC00E71D024714&id=79CC00E71D024714%211288&parId=79CC00E71D024714%211282&o=OneUp
//! most is copy pasted - but 90% of the comments are mine to show I understand the code correctly
//! my comments are marked !
//! i added an .h and now to run send_char() to send a char you must first run setup_sender(), netlink_open_and_bind and setup_msghdr
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/udp.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>

struct ifreq ifreq_c, ifreq_i, ifreq_ip; /// for each ioctl keep diffrent ifreq structure otherwise error may come in sending(sendto )
int sock_raw;							 //! the socket that sends stuff
unsigned char *sendbuff;				 //! msg buffer

//! destination mac and ip addresses
#define DESTMAC0 0x08
#define DESTMAC1 0x00
#define DESTMAC2 0x27
#define DESTMAC3 0xa3
#define DESTMAC4 0x42
#define DESTMAC5 0xb2

#define destination_ip 192.168.227.198

//! total_len - the total size of the full packet; from ethernet header all the way to the data itself
int total_len = 0, send_len, ready_to_send = 0;

/*
!
get the ethernet index, using i
I got the interface name using ifconfig
ioctl(sock_raw,SIOCGIFINDEX,&ifreq_i) - from what i get ioctl SIOCGIFINDEX is a system call that
										fills the ifreq_i ifr_ifindex field with
										the index of an internet interface named wlp2s0
*/
void get_eth_index();

//! get my the mac adress using SIOCGIFHWADDR
void get_mac();

//! set the data in the send buffer, as well as adjust the total packet size.
//! as there's only one character to set, this function doesn't do much and therefore inlined
void set_data(char tav);

//!setup the udp header
void get_udp(char data);
//calculate checksum as by a formula
unsigned short checksum(unsigned short *buff, int _16bitword);
//!set up the ip header
void get_ip();
//! create raw socket, data buffer, ethernet header and ip header
void setup_sender();

void send_char(char tav);

// open and bind the netlink socket that shall recieve and sent to the kernel
// returns the new socket
int netlink_open_and_bind();

// setup various structs needed for netlink communication.
// as the kernel cannot send data by itself whenever it wants, we shall ask the kernel for a message by sending our own message first,
// and awaiting for a response. here we setup the pointers to the msgheaders that will store both the sent and the received message.
// struct nlmsghdr *nlh/2 - header struct to setup - will be filled with request/response settings. also stores the actual message.
// struct msghdr *msg/resp - header struct to setup - will be sent/received to/from the netlink
// doesn't return anything
void setup_msg_hdr(struct nlmsghdr **nlh2, struct nlmsghdr **nlh,struct msghdr *msg,struct msghdr *resp);