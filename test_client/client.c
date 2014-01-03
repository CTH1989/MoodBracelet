#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#define MAX(a,b) (a)>(b)?(a):(b)

int main()
{
     int s,connectReturn, maxfd;
  fd_set rset;
     char sendbuf[1024] = {0};
  char recvbuf[1024] = {0};
     long port=5358;
      s=socket(AF_INET,SOCK_STREAM,0);

      struct sockaddr_in sa;
      sa.sin_family=AF_INET;
      sa.sin_addr.s_addr=inet_addr("192.168.1.66");
      sa.sin_port=htons(port);
  connectReturn=connect(s,(struct sockaddr *)&sa,sizeof(sa));
      printf("%d\n",connectReturn);
  FD_ZERO(&rset);
  while(1)
  {
   FD_SET(fileno(stdin), &rset);
   FD_SET(s, &rset);
   maxfd=MAX(fileno(stdin), s) + 1;

   select(maxfd, &rset, NULL, NULL, NULL);
   if(FD_ISSET(fileno(stdin), &rset))
   {
    scanf("%s",sendbuf);
    send(s,sendbuf,strlen(sendbuf),0);
    bzero(sendbuf, 1024);
   }
   else if(FD_ISSET(s, &rset))
   {
    memset(recvbuf,0,1024);
    recv(s,recvbuf,1024,0);
    printf("remote: %s\n",recvbuf);
   }
  }
      return 0;
}
