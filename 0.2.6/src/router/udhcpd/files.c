/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */
 
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"

char char_to_data(const char ch)
{
    switch(ch){
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a':
		case 'A': return 10;
		case 'b':
		case 'B': return 11;
		case 'c':
		case 'C': return 12;
		case 'd':
		case 'D': return 13;
		case 'e':
		case 'E': return 14;
		case 'f':
		case 'F': return 15;
    }
    return 0;
}

static void mac_string_to_value(unsigned char *mac,unsigned char *buf)
{
    int i;
    int len;
	unsigned char * p_temp = mac;

	if(mac && buf){
		len = strlen((const char *)mac);
		for (i=0;i<(len-5)/2;i++){
			//mach_len = sscanf((const char *)mac+i*3,"%2x",&buf[i]);

			buf[i] = char_to_data(*p_temp++) * 16;
			buf[i] += char_to_data(*p_temp++);
			p_temp++;
		}
	}
}

static int is_ip(const char *str)
{
    struct in_addr addr;
    int ret;

		if (str == NULL)
			return -1;
    ret = inet_pton(AF_INET, str, &addr);
    return ret;
}

/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;

	if (!inet_aton(line, addr)) {
		if ((host = gethostbyname(line))) 
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}


static int read_str(char *line, void *arg)
{
	char **dest = arg;
	
	if (*dest) free(*dest);
	*dest = strdup(line);
	
	return 1;
}


static int read_u32(char *line, void *arg)
{
	u_int32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
}


static int read_yn(char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;
	
	return retval;
}


/* read a dhcp option and add it to opt_list */
static int read_opt(char *line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	struct dhcp_option *option = NULL;
	int retval = 0, length = 0;
	char buffer[255];
	u_int16_t result_u16;
	u_int32_t result_u32;
	int i;

	if (!(opt = strtok(line, " \t="))) return 0;
	
	for (i = 0; options[i].code; i++)
		if (!strcmp(options[i].name, opt))
			option = &(options[i]);
		
	if (!option) return 0;
	
	do {
		val = strtok(NULL, ", \t");
		if (val) {
			length = option_lengths[option->flags & TYPE_MASK];
			retval = 0;
			switch (option->flags & TYPE_MASK) {
			case OPTION_IP:
				retval = read_ip(val, buffer);
				break;
			case OPTION_IP_PAIR:
				retval = read_ip(val, buffer);
				if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
				if (retval) retval = read_ip(val, buffer + 4);
				break;
			case OPTION_STRING:
				length = strlen(val);
				if (length > 0) {
					if (length > 254) length = 254;
					memcpy(buffer, val, length);
					retval = 1;
				}
				break;
			case OPTION_BOOLEAN:
				retval = read_yn(val, buffer);
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, &endptr, 0);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S16:
				result_u16 = htons(strtol(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S32:
				result_u32 = htonl(strtol(val, &endptr, 0));	
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			default:
				break;
			}
			if (retval) 
				attach_option(opt_list, option, buffer, length);
		};
	} while (val && retval && option->flags & OPTION_LIST);
	return retval;
}


static struct config_keyword keywords[] = {
	/* keyword[14]	handler   variable address		default[20] */
	{"start",	read_ip,  &(server_config.start),	"192.168.0.20"},
	{"end",		read_ip,  &(server_config.end),		"192.168.0.254"},
	{"interface",	read_str, &(server_config.interface),	"eth0"},
	{"option",	read_opt, &(server_config.options),	""},
	{"opt",		read_opt, &(server_config.options),	""},
	{"max_leases",	read_u32, &(server_config.max_leases),	"254"},
	{"remaining",	read_yn,  &(server_config.remaining),	"yes"},
	{"auto_time",	read_u32, &(server_config.auto_time),	"7200"},
	{"decline_time",read_u32, &(server_config.decline_time),"3600"},
	{"conflict_time",read_u32,&(server_config.conflict_time),"3600"},
	{"offer_time",	read_u32, &(server_config.offer_time),	"60"},
	{"min_lease",	read_u32, &(server_config.min_lease),	"60"},
	{"lease_file",	read_str, &(server_config.lease_file),	"/var/lib/misc/udhcpd.leases"},
	{"pidfile",	read_str, &(server_config.pidfile),	"/var/run/udhcpd.pid"},
	{"notify_file", read_str, &(server_config.notify_file),	""},
	{"siaddr",	read_ip,  &(server_config.siaddr),	"0.0.0.0"},
	{"sname",	read_str, &(server_config.sname),	""},
	{"boot_file",	read_str, &(server_config.boot_file),	""},
	/*ADDME: static lease */
	{"",		NULL, 	  NULL,				""}
};


int read_config(char *file)
{
	FILE *in;
	char buffer[80], orig[80], *token, *line;
	int i;

	for (i = 0; strlen(keywords[i].keyword); i++)
		if (strlen(keywords[i].def))
			keywords[i].handler(keywords[i].def, keywords[i].var);

	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}
	
	while (fgets(buffer, 80, in)) {
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
		strncpy(orig, buffer, 80);
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		
		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line); i > 0 && isspace(line[i - 1]); i--);
		line[i] = '\0';
		
		for (i = 0; strlen(keywords[i].keyword); i++)
			if (!strcasecmp(token, keywords[i].keyword))
				if (!keywords[i].handler(line, keywords[i].var)) {
					LOG(LOG_ERR, "unable to parse '%s'", orig);
					/* reset back to the default value */
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
	}
	fclose(in);
	return 1;
}


void write_leases(void)
{
	FILE *fp,*fp_static;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
	
	
	if (!(fp = fopen(server_config.lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.lease_file);
		return;
	}
	
	if (!(fp_static = fopen(static_lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", static_lease_file);
		return;
	}
	for (i = 0; i < server_config.max_leases; i++) {
		if (leases[i].yiaddr != 0 && leases[i].flag !=1) {
			if (server_config.remaining) {
				if (lease_expired(&(leases[i])))
					lease_time = 0;
				else lease_time = leases[i].expires - curr;
			} else lease_time = leases[i].expires;
			lease_time = htonl(lease_time);
			fwrite(leases[i].chaddr, 16, 1, fp);
			fwrite(&(leases[i].yiaddr), 4, 1, fp);
			fwrite(&lease_time, 4, 1, fp);
			fwrite(leases[i].hostname, 64, 1, fp);
		}else{
			if (server_config.remaining) {
				if (lease_expired(&(leases[i])))
					lease_time = 0;
				else lease_time = leases[i].expires - curr;
			} else lease_time = leases[i].expires;
			lease_time = htonl(lease_time);
			fwrite(leases[i].chaddr, 16, 1, fp_static);
			fwrite(&(leases[i].yiaddr), 4, 1, fp_static);
			fwrite(&lease_time, 4, 1, fp_static);
			fwrite(leases[i].hostname, 64, 1, fp_static);
		}
	}
	fclose(fp);
	fclose(fp_static);
	
	if (server_config.notify_file) {
		sprintf(buf, "%s %s", server_config.notify_file, server_config.lease_file);
		system(buf);
	}
}


void read_leases(char *file)
{
	FILE *fp;
	unsigned int i = 0;
	struct dhcpOfferedAddr lease, *oldest;
	
	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}
	
	while (i < server_config.max_leases && (fread(&lease, sizeof lease, 1, fp) == 1)) {
		/* ADDME: is it a static lease */
		if (ntohl(lease.yiaddr) >= ntohl(server_config.start) 
		&&  ntohl(lease.yiaddr) <= ntohl(server_config.end) ){
			lease.expires = ntohl(lease.expires);
			if (!server_config.remaining) lease.expires -= time(0);
			if (!(oldest = add_lease(lease.chaddr, lease.yiaddr, lease.expires))) {
				LOG(LOG_WARNING, "Too many leases while loading %s\n", file);
				break;
			}				
			strncpy(oldest->hostname, lease.hostname, sizeof(oldest->hostname) - 1);
			oldest->hostname[sizeof(oldest->hostname) - 1] = '\0';
			i++;
		}
	}
	DEBUG(LOG_INFO, "Read %d leases", i);
	fclose(fp);
}

void add_static_leases()
{
	unsigned int i = 1;
	unsigned int static_num = 0;
	char *static_ip_num = NULL;
	char *static_ip_mac = NULL;
	char *static_ip = NULL;
	FILE *fp;

	char key[64] = {'\0'};
	char *p_value = NULL;
	unsigned char mac[6] = {0};
	u_int32_t ip_addr;
	struct dhcpOfferedAddr lease, *oldest;

	LOG(LOG_INFO, "udhcp %s %d\n", __FUNCTION__,__LINE__);
	char shell_cmd[128] = {'\0'};
	system("touch /tmp/log_dhcpd");

	static_ip_num = nvram_safe_get("static_ip_num");
	
	if(static_ip_num == NULL){
		return;
	}
	static_num = atoi(static_ip_num);
	LOG(LOG_INFO, "udhcp %s %d static_num:%d\n", __FUNCTION__,__LINE__,static_num);
	if(atoi(static_ip_num) >= server_config.max_leases){
		return;
	}

	if (!(fp = fopen("/tmp/log_dhcpd", "w"))) {
		return;
	}
	LOG(LOG_INFO, "udhcp %s %d\n", __FUNCTION__,__LINE__);
	system(shell_cmd);
	for(i = 1;i<=static_num;i++){
		memset(mac,0,sizeof(mac));
		/*get the mac address*/
		memset(key,0,sizeof(key));
		sprintf(key,"static_ip_mac%d",i);
		p_value = NULL;
		p_value = nvram_safe_get(key);
		mac_string_to_value(p_value,mac);
		memcpy(lease.chaddr,mac,6);
		/*get the ip address*/
		memset(key,0,sizeof(key));
		sprintf(key,"static_ip%d",i);
		p_value = NULL;
		p_value = nvram_safe_get(key);
		lease.yiaddr = inet_addr(p_value);
		LOG(LOG_INFO, "udhcp %s %d mac:%02x:%02x:%02x:%02x:%02x:%02x ip:%u\n", __FUNCTION__,__LINE__,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],lease.yiaddr);
		/*init the hostname*/
		strcpy(lease.hostname,"static");
		LOG(LOG_INFO, "udhcp %s %d start:%u end:%u act:%u\n", __FUNCTION__,__LINE__,ntohl(server_config.start),ntohl(server_config.end),ntohl(lease.yiaddr));
		/* ADDME: is it a static lease */
		if (find_lease_by_chaddr(lease.chaddr)!=NULL){
			continue;
		}

		if (ntohl(lease.yiaddr) >= ntohl(server_config.start) 
		&&  ntohl(lease.yiaddr) <= ntohl(server_config.end) ){
			lease.expires = ntohl(LEASE_TIME);
			if (!server_config.remaining) lease.expires -= time(0);
			if (!(oldest = add_lease(lease.chaddr, lease.yiaddr, lease.expires))) {
				LOG(LOG_INFO,"add failed %\n");
				break;
			}
			
			oldest->flag = 1;
			strncpy(oldest->hostname, lease.hostname, sizeof(oldest->hostname) - 1);
			oldest->hostname[sizeof(oldest->hostname) - 1] = '\0';
			i++;
		}
		i++;
	}
		fclose(fp);
}	
		
