/* files.h */
#ifndef _FILES_H
#define _FILES_H

#define static_lease_file "/tmp/udhcpd0_static.leases"
struct config_keyword {
	char keyword[14];
	int (*handler)(char *line, void *var);
	void *var;
	char def[30];
};


int read_config(char *file);
void write_leases(void);
void read_leases(char *file);

#endif
