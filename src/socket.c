#include "main.h"

/*********************************************/
// controlla se il protocollo � HTTP e la risposta 200

int check_data(char *Buffer) {
  int i;
  char *ptr = Buffer, *protocol = NULL, *response = NULL;

  for (i = 0; i < 2; i++) {
    switch (i) {
      case 0: protocol = ptr;
        break;
      case 1: response = ptr;
        break;
      default: break;
    }
    ptr = strchr(ptr, ' ');
    if (!ptr) return 0;
    *ptr++ = '\0';
  }
  if (strncmp(protocol, "HTTP/", 5) != 0) return 0;
  if (response == NULL || atoi(response) != 200) return 0;
  return 1;
}

/*********************************************/
// crea un file unix e ne ritorna il descrittore

int create_file(char *filename, int mask) {
//  char *mark = TEMP_MARK;
  char *overwrite_str;
  int fd = NO_FILE, overwrite;
  mode_t u_mask;
  static char template[] = "ezinXXXXXX";

  overwrite_str = getfieldbyname("overwrite");
  overwrite = (overwrite_str && strcasecmp(overwrite_str, "on") == 0) ? 1 : 0;

  u_mask = umask(0);
  if (filename && access(filename, F_OK) == 0 && overwrite == 0) {
    umask(u_mask);
    globaldata.gd_error = mysprintf(_( "File \"%s\" already exists!<br />Please go back to overwrite it<br /><br />"), filename);
    return fd;
  }
  if (filename) fd = open(filename, O_CREAT | O_RDWR, mask);
  else {
    fd = mkstemp(template);
    globaldata.gd_inifile = template;
    //if (fd!=NO_FILE) write(fd,mark,strlen(mark)); // mark the file as an easinstall temporary file...
  }
  if (fd == NO_FILE) {
    //printf("File \"%s\" can't be created<br />Please check permissions",filename);
    umask(u_mask);
    return fd;
  }
  umask(u_mask);
  return fd;
}

// Scarica un file remoto di nome remote_file
// dall'host di nome remote_host
// gli da' nome filename con i permessi unix mask

int Download(int hSocket, char *remote_host, char *remote_file, char *filename, int mask) {
  int fd = NO_FILE;
  static char StrBuf[DATBUF_SIZE];
  static char Buffer[DATBUF_SIZE];
  char *dat = NULL;
  int rc = 0, done = 0;

  if (hSocket == NO_SOCKET) return rc;

  //	if (!remote_host || !*remote_host || !remote_file || !*remote_file || !filename || !*filename)
  if (!remote_host || !*remote_host || !remote_file || !*remote_file || (filename && !*filename))
    return 0;

  sprintf(StrBuf, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Ezinstall downloader/0.1\r\n\r\n", remote_file, remote_host);

  if (!write(hSocket, StrBuf, strlen(StrBuf))) return 0;

  while ((rc = read(hSocket, Buffer, DATBUF_SIZE - 1)) > 0) {
    char *buf = Buffer;
    int len;
    if (!done) {
      //dat=memmem(Buffer,rc,"\r\n\r\n",4);
      dat = strstr(Buffer, "\r\n\r\n");
      if (!dat) break;
      *dat = '\0';
      len = strlen(Buffer) + 4;
      rc -= len;
      buf = &dat[4];
      done = check_data(Buffer);
      if (done) fd = create_file(filename, mask);
      else Error(_( "Can't download data"));
      if (fd == NO_FILE) {
        Error(_( "Can't create archive file"));
      }
    }
    write(fd, buf, rc);
  }
  // if temp file write a newline at end of file
  if (!filename && fd != NO_FILE) write(fd, "\n", 1);

  if (fd != NO_FILE) close(fd);

  return done;
}

/***************************************************/

// Apre connessione di rete a hostname oppure all'IP ip
// port specifica la porta e nonblock se attivato il NONBLOCK

int OpenConnection(char *hostname, unsigned long ip, int port, int nonblock) {
  struct hostent *HostAddr;
  struct sockaddr_in INetSocketAddr;
  int hSocket;

  memset((char *) &INetSocketAddr, 0, sizeof (INetSocketAddr));

  if (hostname && *hostname) {
    HostAddr = (struct hostent *) gethostbyname(hostname);
    if (!HostAddr) {
      Error(_("http domain not found"));
    }
    memcpy((char *) &INetSocketAddr.sin_addr, HostAddr->h_addr, HostAddr->h_length);
  } else {
    HostAddr = (struct hostent *) gethostbyaddr((char *) &ip, 4, PF_INET);
    if (!HostAddr) {
      Error(_("ip address not found"));
    }
    memcpy((char *) &INetSocketAddr.sin_addr, HostAddr->h_addr, HostAddr->h_length);
  }

  INetSocketAddr.sin_family = AF_INET; //HostAddr->h_addrtype;
  INetSocketAddr.sin_port = htons(port);

  hSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (!hSocket) {
    Error(_("can't alloc socket"));
  }

  if (connect(hSocket, (struct sockaddr *) &INetSocketAddr, sizeof (INetSocketAddr)) == -1) {
    Error(_("can't connect to the http host"));
  }

  if (nonblock) {
#ifdef O_NONBLOCK
    if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == -1L) {
      Error(_("can't set nonblock io"));
    }
#else
    if (ioctlsocket(hSocket, FIONBIO, (void *) &one) == -1L) {
      Error(_("can't set nonblock io"));
    }
#endif
  }

  return (hSocket);
}

void CloseConnection(SOCKET sock) {
  int nRet;
  static char szBuf[255];

  // Tell the remote that we're not going
  // to send any more data
  shutdown(sock, 1);
  while (1) {
    // Try to receive data.
    // This clears any data that
    // may still be buffered in
    // the protocol stack
    nRet = recv(
            sock, // socket
            szBuf, // data buffer
            sizeof (szBuf), // length of buffer
            0); // recv flags
    // Stop receiving data if connection
    // closed or on any other error.
    if (nRet == 0 || nRet == -1)
      break;
  }
  // Tell the peer that we're not
  // going to receive any more either.
  shutdown(sock, 2);
  // Close the socket
  closesocket(sock);
}
