#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define BUFLEN 1024
#define GROUPLEN 5
#define SOCKETLEN 5
#define SOCKETNUM 100
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define BOOL int
#define TRUE 1
#define FALSE 0

char buf[BUFLEN];
char rbuf[BUFLEN];
int connfd,listenfd;

typedef struct
{
    int socketfd[SOCKETLEN];
    int color_index;
}Group;

int socket_counter;

void str_echo(FILE *, int, int *, int *, Group *);
void sig_chld(int);
BOOL insert(int, int, int *, Group *);

int main()
{
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr,servaddr;
    
    int shmid, i, j, connfd_t, shmid_group;
    int *shmaddr, *shmptr;
    Group *shmaddr_group, *shmptr_group;
    connfd_t = -1;
    
    // Create a shared memory
    // Shared memory for socket array
    if ((shmid = shmget(IPC_PRIVATE, (sizeof(int) * SOCKETNUM), IPC_CREAT|0600)) < 0)
    {
        perror("Failed to create shared memory.\n");
        return -1;
    }
    
    shmaddr = (int *)shmat(shmid, NULL, 0);
    shmptr = shmaddr;
    for (i = 0; i < SOCKETNUM; i ++)
    {
        (*shmptr) = -1;
    }
    
    // Shared memory for group
    if ((shmid_group = shmget(IPC_PRIVATE, (sizeof(Group) * GROUPLEN), IPC_CREAT|0600)) < 0)
    {
        perror("Failed to create shared memory.\n");
        shmdt(shmaddr);
        shmctl(shmid, IPC_RMID, NULL);
        return -1;
    }
    
    shmaddr_group = (Group *)shmat(shmid_group, NULL, 0);
    shmptr_group = shmaddr_group;
    for (i = 0; i < GROUPLEN; i ++)
    {
        (shmptr_group + i) -> color_index = -1;
        int *ptr = (shmptr_group + i) -> socketfd;
        for (j = 0; j < SOCKETLEN; j ++)
        {
            *(ptr + j) = -1;
        }
    }
    
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket.");
        shmdt(shmaddr);
        shmctl(shmid, IPC_RMID, NULL);
        shmdt(shmaddr_group);
        shmctl(shmid_group, IPC_RMID, NULL);
        return -1;
    }

    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(5358);

    if ((bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr))) < 0)
    {
        perror("Failed to bind socket.");
        shmdt(shmaddr);
        shmctl(shmid, IPC_RMID, NULL);
        shmdt(shmaddr_group);
        shmctl(shmid_group, IPC_RMID, NULL);
        return -1;
    }
    
    listen(listenfd, 5);

    signal(SIGCHLD, sig_chld);

    while(TRUE)
    {
        clilen = sizeof(cliaddr);

        shmaddr = (int *)shmat(shmid, NULL, 0) ;            
        shmptr = shmaddr;
        for (i = 0; i < SOCKETNUM; i ++)
        {
            if ((*(shmptr + i)) == -1) connfd_t = i;
        }
    
        if (connfd_t == -1)
        {
            perror("Socket list is full.");
            continue ;
        }
        
        if(((*(shmaddr + connfd_t)) = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) < 0)
        {
            perror("Accept error.");
            if(errno == EINTR) continue;
        }

        if((childpid = fork()) == 0)
        {
            int paired = -1;
            int index = -1;
        
            close(listenfd);
            

            connfd = (*(shmaddr + connfd_t));
            shmaddr_group = (Group *)shmat(shmid_group, NULL, 0);
            
            printf("Connection from %s. with socket %d\n", inet_ntoa(cliaddr.sin_addr), connfd);
            str_echo(stdin, connfd, &paired, &index, shmaddr_group);
            (shmaddr_group + paired) -> socketfd[index] = -1;
            
            shmdt(shmaddr);
            shmdt(shmaddr_group);
            close(connfd);
            exit(0);
        }
    }
}

void str_echo(FILE *fp, int sockfd, int *paired, int *id, Group *shm_group)
{
    int n = 0, i;
    char sendbuf[BUFLEN] = {0}, recvbuf[BUFLEN] = {0};
    int maxfdp;
    fd_set rset;

    FD_ZERO(&rset);

    while(TRUE)
    {
        FD_SET(fileno(fp),&rset);
        FD_SET(sockfd, &rset);
        maxfdp = MAX(fileno(fp), sockfd) + 1;

        select(maxfdp, &rset ,NULL, NULL, NULL);
  
        if(FD_ISSET(sockfd, &rset))
        {    
            if(n = read(sockfd, recvbuf, BUFLEN) == 0)
            {
                return;
            }
            if(n == -1)
            {
                break;
            }
            
            printf("Receive: %s status:", recvbuf);
            if ((*paired) == -1) printf("Not paired.\n");
            else printf("Paired in group %d at %d.\n", (*paired), (*id));
            
            if (recvbuf[0] == 'p' && (*paired) == -1)
            {
                int tmp = recvbuf[1] - '0';
                if (insert(tmp, sockfd, id, shm_group))
                {
                    printf("Insert to group %d at %d.\n", tmp, (*id));
                    (*paired) = tmp;
                    memset(sendbuf, 0, sizeof(sendbuf));
                    sendbuf[0] = 'y';
                    sendbuf[1] = '\n';
                    write(sockfd, sendbuf, strlen(sendbuf));
                }
                else
                {
                    printf("Failed to get bracelet paired.\n");
                    memset(sendbuf, 0, sizeof(sendbuf));
                    sendbuf[0] = 'n';
                    sendbuf[1] = '\n';
                    write(sockfd, sendbuf, strlen(sendbuf));
                    return;
                }
                    
            }
            
            if (recvbuf[0] != 'p' && (*paired) != -1)
            {
                (shm_group + (*paired)) -> color_index = recvbuf[0] - '0';
                memset(sendbuf, 0, sizeof(sendbuf));
                sendbuf[0] = recvbuf[0];
                sendbuf[1] = '\n';
                for (i = 0; i < SOCKETLEN; i ++)
                {
                    if ((shm_group + (*paired)) -> socketfd[i] != -1)
                    {
                        printf("Send data to %d.\n", (shm_group + (*paired)) -> socketfd[i]);
                        send((shm_group + (*paired)) -> socketfd[i], sendbuf, strlen(sendbuf), 0);
                    }
                }
            }
        }
        if(FD_ISSET(fileno(fp), &rset))
        {
            memset(sendbuf, 0, sizeof(sendbuf));
            scanf("%s",sendbuf);
            if (sendbuf[0] == 'c') printf("fuck");
            sendbuf[strlen(sendbuf)] = '\n';
            write(sockfd, sendbuf, strlen(sendbuf));
        }
    }
}

BOOL insert(int id, int sockfd, int *index, Group *shmptr)
{
    int i;
    if (id >= GROUPLEN) return FALSE;
    for (i = 0; i < SOCKETLEN; i ++)
    {
        if ((shmptr + id) -> socketfd[i] == -1)
        {
            (shmptr + id) -> socketfd[i] = sockfd;
            (*index) = i;
            return TRUE;
        }
    }
    return FALSE;
}

void sig_chld(int signo)
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        printf("Child %d terminated.\n", pid);
    }
    
    return ;
}
