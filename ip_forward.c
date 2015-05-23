#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <errno.h>
#include <string.h>

#define MAX_BUFFER 2048
#define MAX_ROUTE_INFO 10
#define MAX_ARP_SIZE 10
#define DEBUG

//the information of the static_routing_table
struct route_item{
	char destination[16];		
	char gateway[16];		
	char netmask[16];		
	int interface;
}route_info[MAX_ROUTE_INFO];	
//the number of the items in the routing table
int route_item_index=0;			

//the information of the arp_cache
struct arp_table_item{
	char ip_addr[16];		
	char mac_addr[18];		
}arp_table[MAX_ARP_SIZE];	
//the number of the items in the arp cache	
int arp_item_index=0;			

//Read the static route table.
void read_route_table(){
	if(route_item_index != 0){
		return;
	}
	FILE* file = fopen("route_table", "r");
	if(file == NULL){
		printf("Cannot read route table.\n");
		exit(0);
	}
	if(fseek(file, 24, SEEK_SET) != 0){
		perror("Reading route table");
		exit(0);
	}
	char buffer[20];
	int ifnum;
	fscanf(file, "%s", buffer); 
	while(!feof(file)){
		strcpy(route_info[route_item_index].destination, buffer);
		fscanf(file, "%s", buffer);
		strcpy(route_info[route_item_index].gateway, buffer);
		fscanf(file, "%s", buffer);
		strcpy(route_info[route_item_index].netmask, buffer);
		fscanf(file, "%d", &ifnum);
		route_info[route_item_index].interface = ifnum;
		route_item_index++;
		//Read the next line
		fscanf(file, "%s", buffer); 
	}
	fclose(file);
	route_item_index = route_item_index<=MAX_ROUTE_INFO?route_item_index:MAX_ROUTE_INFO;
}

void read_arp_table(){
	if(arp_item_index != 0){
		return;
	}
	FILE* file = fopen("arp_table", "r");
	if(file == NULL){
		printf("Cannot read ARP table.\n");
		exit(0);
	}
	if(fseek(file, 11, SEEK_SET) != 0){
		perror("Reading ARP table");
		exit(0);
	}
	char buffer[20];
	fscanf(file, "%s", buffer); 
	while(!feof(file)){
		strcpy(arp_table[arp_item_index].ip_addr, buffer);
		fscanf(file, "%s", buffer);
		strcpy(arp_table[arp_item_index].mac_addr, buffer);
		arp_item_index++;
		//Reading the next line.
		fscanf(file, "%s", buffer);
	}
	fclose(file);
	arp_item_index = arp_item_index<=MAX_ARP_SIZE?arp_item_index:MAX_ARP_SIZE;
	if(arp_item_index < 2){
		printf("Reading ARP table: cannot read enough lines.\n");
		exit(0);
	}
}

int find_arp_by_mac(char* mac_addr){
	int i=0;
	for(i=0; i<arp_item_index; i++){
		if(strcmp(mac_addr, arp_table[i].mac_addr) == 0){
			return i;
		}
	}
	return -1;
}

int find_arp_by_ip(char* ip_addr){
	int i=0;
	for(i=0; i<arp_item_index; i++){
		if(strcmp(ip_addr, arp_table[i].ip_addr) == 0){
			return i;
		}
	}
	return -1;
}

char* find_route_gateway(char* ip_addr){
	static char gateway[16];
	memset(gateway, 0, 16);
	int i;
	for(i=0; i<route_item_index; i++){
		//Just ignore the input netmask and compare the first 9 char ^_^.
		if(strncmp(ip_addr, route_info[i].destination, 9) == 0){
			strcpy(gateway, route_info[i].gateway);
			return gateway;
		}
	}
	printf("Broken route table.\n");
	exit(0);
	return NULL;
}

void refresh_arp(char* ip_addr, char* mac_addr){
	if(find_arp_by_ip(ip_addr) != -1){
		return;
	}
	if(arp_item_index == MAX_ARP_SIZE){
		arp_item_index--;
	}
	strcpy(arp_table[arp_item_index].ip_addr, ip_addr);
	strcpy(arp_table[arp_item_index].mac_addr, mac_addr);
	arp_item_index++;
	char* gateway = find_route_gateway(ip_addr);
	if(find_arp_by_ip(gateway) != -1){
		return;
	}
	int i = arp_item_index;
	if(arp_item_index == MAX_ARP_SIZE){
		i = i-2;
	}
	strcpy(arp_table[i].ip_addr, gateway);
	strcpy(arp_table[i].mac_addr, mac_addr);
}

int main(){
	//Init tables and variables.
	int i;
	int sock_fd;
	int n_read = 0;
	int src_route_index = -1;
	int des_route_index = -1;
	int src_arp_index = -1;
	int des_arp_index = -1;
	char buffer[MAX_BUFFER];
	char type[4];
	char src_ip[16];
	char des_ip[16];
	char gateway[16];
	char src_mac[18];
	char des_mac[18];
	char device[10];
	char write[6];
	char* p;
	char* ip_head;
	char* eth_head;
	unsigned count=0;
	memset(src_ip, 0, 16);
	memset(des_ip, 0, 16);
	memset(gateway, 0, 16);
	memset(src_mac, 0, 18);
	memset(des_mac, 0, 18);
	memset(device, 0, 10);
	read_route_table();
	read_arp_table();
	sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	if(sock_fd < 0){
		perror("Creating raw socket");
		exit(0);
	}
#ifdef DEBUG
	printf("\n============= Debug Information =============\n");
	printf("Static route table\n");
	for(i=0; i<route_item_index; i++){
		printf("%s %s %s %d\n", 
			route_info[i].destination,
			route_info[i].gateway,
			route_info[i].netmask,
			route_info[i].interface
		);
	}
	printf("\nARP table\n");
	for(i=0; i<arp_item_index; i++){
		printf("%s %s\n",
			arp_table[i].ip_addr,
			arp_table[i].mac_addr
		);
	}
	printf("===============================================\n\n");
#endif
	//Start working
	while(1){
		//Get a message from the raw socket.
		n_read = recvfrom(sock_fd, buffer, MAX_BUFFER, 0, NULL, NULL);
		if(n_read < 64){
			if(n_read < 0){
				perror("Receiving message");
			} else {
				printf("Receiving message: Too short.\n");
			}
			continue;
		}
		//Read the eth head.
		count++;
		eth_head = buffer;
		p = eth_head;
#ifdef DEBUG
		printf("Start reading eth head.\n");
#endif
		sprintf(des_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			p[0]&0xff, p[1]&0xff,
			p[2]&0xff, p[3]&0xff,
			p[4]&0xff, p[5]&0xff
		);
		p += 6;
		sprintf(src_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			p[0]&0xff, p[1]&0xff,
			p[2]&0xff, p[3]&0xff,
			p[4]&0xff, p[5]&0xff
		);
		p += 6;
		sprintf(type, "%02x%02x", p[0]&0xff, p[1]&0xff);
		if(strncmp(type, "0800", 4) != 0){
			printf("Unrecognized datagram: 0x%s\n", type);
			continue;
		}
#ifdef DEBUG
		printf("Finish reading eth head.\n");
		printf("Destination MAC: %s\n", des_mac);
		printf("Source MAC:      %s\n", src_mac);
		printf("Start reading IP head.\n");
#endif
		//Read the IP head.
		ip_head = eth_head+14;
		p = ip_head+12;
		sprintf(src_ip, "%d.%d.%d.%d", 
			p[0]&0xff, p[1]&0xff,
			p[2]&0xff, p[3]&0xff
		);
		p += 4;
		sprintf(des_ip, "%d.%d.%d.%d",
			p[0]&0xff, p[1]&0xff,
			p[2]&0xff, p[3]&0xff
		);
#ifdef DEBUG
		printf("Finish reading IP head.\n");
		printf("Destination IP: %s\n", des_ip);
		printf("Source IP:      %s\n", src_ip);
		printf("Start refreshing ARP table and checking local or not.\n");
#endif
		refresh_arp(src_ip, src_mac);
		src_arp_index = find_arp_by_ip(src_ip);
		des_arp_index = find_arp_by_ip(des_ip);
		if( src_arp_index == 0 ||
			src_arp_index == 1 ||
			des_arp_index == 0 ||
			des_arp_index == 1 ){
			//This is a local packet.
			continue;
		}
#ifdef DEBUG
		printf("This is not local.\n");
		printf("Start checking the static ip table.\n");
#endif
		strcpy(gateway, find_route_gateway(des_ip));
		des_arp_index = find_arp_by_ip(gateway);
		if(des_arp_index == 0 || des_arp_index == 1){
			des_arp_index = find_arp_by_ip(des_ip);
		}
		if(des_arp_index == -1){
			strncpy(des_mac, "ff:ff:ff:ff:ff:ff", 17);
		} else {
			strncpy(des_mac, arp_table[des_arp_index].mac_addr, 17);
		}
#ifdef DEBUG
		printf("Get destination gateway and MAC.\n");
		printf("Gateway: %s\n", gateway);
		printf("MAC:     %s\n", des_mac);
		printf("Start sending.\n");
#endif
		strcpy(gateway, find_route_gateway(gateway));
		src_arp_index = find_arp_by_ip(gateway);
		switch(src_arp_index){
		case 0:
			strcpy(src_mac, arp_table[0].mac_addr);
			sprintf(device, "eth%d", 0);
			break;
		case 1:
			strcpy(src_mac, arp_table[1].mac_addr);
			sprintf(device, "eth%d", 1);
			break;
		default:
			printf("Unknown gateway.\n");
			exit(0);
		}
		sscanf(des_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			&write[0], &write[1], &write[2],
			&write[3], &write[4], &write[5]
		);
		for(i=0; i<6; i++){
			eth_head[i] = write[i];
		}
		sscanf(src_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			&write[0], &write[1], &write[2],
			&write[3], &write[4], &write[5]
		);
		for(i=0; i<6; i++){
			eth_head[6+i] = write[i];
		}
		//Just the final step
		struct ifreq ifrq;
		struct sockaddr_ll addr;
		strcpy(ifrq.ifr_name, device);
		ioctl(sock_fd, SIOCGIFINDEX, &ifrq);
		addr.sll_ifindex = ifrq.ifr_ifindex;
		addr.sll_family = PF_PACKET;
		if(sendto(sock_fd, buffer, n_read, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0){
			perror("Sending: ");
		}
#ifdef DEBUG
		printf("Packet %d send successfully.\n\n", count);
#endif
	}
	return count;
}
