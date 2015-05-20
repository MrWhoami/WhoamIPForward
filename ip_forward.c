#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <errno.h>
#include <string.h>

#define MAX_BUFFER 2048
#define MAX_ROUTE_INFO 10
#define MAX_ARP_SIZE 10
//#define DEBUG

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

int main(){
	//Init tables and variables.
	int i;
	int sock_fd;
	int n_read = 0;
	int route_index = -1;
	int arp_index = -1;
	char buffer[MAX_BUFFER];
	char type[4];
	char src_ip[16];
	char des_ip[16];
	char src_mac[18];
	char des_mac[18];
	char* p;
	char* ip_head;
	char* eth_head;
	unsigned count=0;
	memset(src_ip, 0, 16);
	memset(des_ip, 0, 16);
	memset(src_mac, 0, 18);
	memset(des_mac, 0, 18);
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
		//Read the IP head.
	}
	return count;
}
