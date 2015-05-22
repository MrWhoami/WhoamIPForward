#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#define MAX_ROUTE_INFO 10
#define MAX_ARP_SIZE 10

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
	int interface;
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
		strcpy(route_info[route_item_index].interface, ifnum);
		route_item_index++;
		//Route Next line
		fscanf(file, "%s", buffer); 
	}
	fclose(file);
	route_item_index = route_item_index<MAX_ROUTE_INFO?route_item_index:MAX_ROUTE_INFO-1;
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
	if(fseek(file, 18, SEEK_SET) != 0){
		perror("Reading ARP table");
		exit(0);
	}
	char buffer[20];
	int ifnum;
	fscanf(file, "%s", buffer); 
	while(!feof(file)){
		strcpy(arp_table[arp_item_index]
	}
}

int main(){
	return 0;
}
