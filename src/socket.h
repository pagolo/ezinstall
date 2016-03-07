#ifndef __SOCKET_H__
#define __SOCKET_H__

#define NO_SOCKET -1
#define NO_FILE NO_SOCKET
#define DATBUF_SIZE 512

int OpenConnection(char *hostname, unsigned long ip, int port, int nonblock);
void CloseConnection(SOCKET sock);
int Download(int hSocket, char *remote_host, char *remote_file, char *filename, int mask, STRING **list);
int create_file(char *filename, int mask);

#endif
