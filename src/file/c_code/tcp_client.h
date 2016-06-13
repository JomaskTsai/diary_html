#ifndef SOCLE_TCP_CLIENT_H_FILE
#define SOCLE_TCP_CLIENT_H_FILE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

int nc_client(char *ip , int port , char *msg);
#endif SOCLE_TCP_CLIENT_H_FILE