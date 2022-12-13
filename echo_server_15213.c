/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h> 
#include <netdb.h> 
#include <arpa/inet.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/stat.h>
#include <time.h>
#include <sys/select.h>


#define ECHO_PORT 9999 /* argv[1] is used instead */
#define BUF_SIZE 4096

typedef struct { /* Represents a pool of connected descriptors */
    int maxfd;        /* Largest descriptor in read_set */
    fd_set master_sockets;  /* Set of all active descriptors */
    fd_set ready_sockets; /* Subset of descriptors ready for reading  */
    int nready;       /* Number of ready descriptors from select */
    int maxi;         /* Highhest index of active client in client array */
    int clientfd[FD_SETSIZE];    /* Set of active descriptors */
} pool;

int byte_cnt = 0; /* Global variable counts total bytes received by the server */

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int sock;                   /* welcome socket descriptor */
//fd_set master_sockets;     /* current clients socket descriptors and welcome socket descriptor */
//fd_set ready_sockets;     /* temp socket descriptor list for select() */
//int maxfd;               /* max number of descriptors in the system */

struct client_state {                /* this link list track the state infor of each active http client */

    int client_socket_fd;           /* client socket file descriptor */
    char req_buf[BUF_SIZE];        /* the client send/recv buffer size */
    struct client_state *next;   /* pointer to next client or NULL */
};

struct client_state *clients_buff_state;


/* get sockadd and IP address*/
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* to help the server to send the entire application message */
int sendall(int s, char *buf, int *len)
{
    int total = 0;              /* no. of bytes sent */
    int bytesleft = *len;      /* no. of bytes left to send */
    int n;
    while(total < *len) {
    n = send(s, buf+total, bytesleft, 0);
    if (n == -1) { break; }
    total += n;
    bytesleft -= n;
}

*len = total;           /* no. of actually sent bytes */
return n==-1?-1:0;     /*  -1 means failure, 0 means success */
}


/* allocation for a newly received client (or connection) */
void allocate_new_client(int connfd, pool *p){

    //struct client_state *temp_client_state = (struct client_state*) calloc (1, sizeof(struct client_state));
    int i;
    p->nready--;    

    for (i = 0; i < FD_SETSIZE; i++) {         /* Find an available slot */
        if (p->clientfd[i] < 0) {
        p->clientfd[i] = connfd;               /* Add connected descriptor to the pool */ 
        FD_SET(connfd, &p->master_sockets);          /* Add the descriptor to descriptor set */
        if (connfd > p->maxfd) {
            p->maxfd = connfd;                   /*Update max descriptor*/
        }
        if (i > p->maxi) {
        p->maxi = i;                          /*Update pool high index*/
        }
        break;
        }
    }
    if (i == FD_SETSIZE) {                     /* Couldn't find an empty slot */
        fprintf(stderr, "add_client error: Too many clients\n");
    }  

    //if (!temp_client_state) {
    //    fprintf(stderr, "Error allocation for a new client.\n");
    //    exit(1);
    //}

    //temp_client_state->client_socket_fd = client_sock;
    //temp_client_state->next = clients_buff_state;
    //clients_buff_state = temp_client_state;

    //FD_SET(client_sock, &master_sockets);   /* add the client to master_sockets set */
    //if (client_sock > maxfd) {             /* keeping track of the maxfd */
    //    maxfd = client_sock;
    //}
}

/* return state information of the client */
struct client_state *get_client_state(int client_sock){

    struct client_state *cli_buff_state = clients_buff_state;
    while(cli_buff_state) {
        if (cli_buff_state->client_socket_fd == client_sock)
            break;
        cli_buff_state = cli_buff_state->next;
    } 
    return cli_buff_state;
}

/* removing a client */
void deallocate_a_client(struct client_state** head_ref, int client_sock)
{

    //struct client_state *cli_buff_state = get_client_state(client_sock);

    //FD_CLR(cli_buff_state->client_socket_fd, &master_sockets); // remove from master_sockets set
    if (close_socket(client_sock)){
        close_socket(sock);
        fprintf(stderr, "*Lisod: error closing client socket*\n"); 
    } 
    
    fprintf(stdout, "Lisod: closing client socket fd %d.\n",client_sock); 

    /* store head node */
    struct client_state *temp = *head_ref, *prev; 
 
    /* if head node itself holds the key to be deleted */
    if (temp != NULL && temp->client_socket_fd == client_sock) {
        *head_ref = temp->next;      /*  changed head */
        free(temp);                  /* free old head */
        return;
    }
 
    /* search for the client_socket_fd to be deleted, keep track of the
    previous node so as to change 'prev->next' */
    while (temp != NULL && temp->client_socket_fd != client_sock) {
        prev = temp;
        temp = temp->next;
    }
 
    /* if key was not present in linked list */
    if (temp == NULL)
        return;
 
    /* unlink the node from linked list */
    prev->next = temp->next;
    
    /* Free memory */
    free(temp); 
}


void manage_client_request(int sock, pool *p){
    // receive data
    int i, client_sock; 
    char buf[BUF_SIZE];
    ssize_t readret;
    readret = 0;

    for (i = 0; i < (i <= p->maxi) && (p->nready > 0); i++) {       or // i <= p->maxfd         /* Find next client to serve */
        client_sock = p->clientfd[i];
        if ((client_sock > 0) && (FD_ISSET(client_sock, &p->ready_sockets))) {   /* If the descriptor is ready, echo */
            p->nready--;
            if ((readret = recv(client_sock, buf, BUF_SIZE, 0)) > 0) {
                byte_cnt += readret;
                printf("Server received %ld (%d total) bytes on fd %d\n", readret, byte_cnt, client_sock);
                if (send(client_sock, buf, readret, 0) != readret) {             /* Echo */  
                close_socket(client_sock);
                close_socket(sock);
                fprintf(stderr, "Error sending to client.\n"); 
                }                                                               /* Potentially exit the server */
                memset(buf, 0, BUF_SIZE);
            }
            else if (readret == 0) {                                            /* EOF detected, remove descriptor */
                if (close_socket(client_sock)) {
                close_socket(sock);
                fprintf(stderr, "Error closing client socket.\n");
                }                                                                 /* Potentially exit the server */
                FD_CLR(client_sock, &p->master_sockets);                                /* Disconnect the client */
                p->clientfd[i] = -1;
            }
            else {                                                              /*error detected in recv */
                close_socket(client_sock);
                close_socket(sock);
                fprintf(stderr, "Error reading from client socket.\n");           
            }                                                                   /* Potentially exit the server */
        }
    }

    //struct client_state *cli_buff_state = get_client_state(client_sock);
    //int clientdata = 0;
    //clientdata = recv(cli_buff_state->client_socket_fd, cli_buff_state->req_buf, BUF_SIZE,0);
    
    //if (clientdata > 1){        
       
    //    fprintf(stdout, "Lisod: %d bytes received from client %d\n", clientdata, client_sock);

    //    if (sendall(cli_buff_state->client_socket_fd, cli_buff_state->req_buf, &clientdata) == -1) { // req_buf is the base address for received bytes 
    //        fprintf(stderr, "Lisod: error sending.\n");
    //        fprintf(stdout, "Lisod: we only sent %d bytes!\n", clientdata);
    //    }

    //    fprintf(stdout, "Lisod: received request from client %d is :%s of size %d.\n", client_sock, cli_buff_state->req_buf, clientdata);
    //}
    //return clientdata;
}


/*arrival of new client */
void *handle_new_client_connection(int sock, pool *p){
    
    int client_sock; 
    struct sockaddr_in cli_addr; /* client address */
    socklen_t cli_size = sizeof(cli_addr);
    char clientIP[INET6_ADDRSTRLEN];

    if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size)) == -1){
        close(sock);
        fprintf(stderr, "Lisod: error accepting a new connection.\n");
        return NULL;
    } else if (client_sock < FD_SETSIZE) { /* check if the newely accepted client socket fd is less than allowed max number of fds for the Lisod */
        
        allocate_new_client(client_sock, p);
        fprintf(stdout,"Lisod: new connection from client %s on "
            "socket %d\n",
            inet_ntop(cli_addr.sin_family, get_in_addr((struct sockaddr*)&cli_addr), clientIP, INET6_ADDRSTRLEN), client_sock);
    }
    else {
        close(client_sock);
        fprintf(stderr, "Lisod: client socket %d exceed allow desriptors limit.\n", client_sock);
        return NULL; 
    }
return NULL;
}

void *handle_data_from_client(int sock, pool *p){

    int readret = 0;
    //readret = manage_client_request(sock, &p);
    if (readret >= BUF_SIZE){
        fprintf(stderr, "client data higher than its allocated buffer.\n");
        //deallocate_a_client(&clients_buff_state,client_sock);
    } else if (readret > 1 && readret < BUF_SIZE){

        //fprintf(stdout, "Lisod: nBytes has sent %d\n",readret);

    }else if (readret < 1 ){
        
       // deallocate_a_client(&clients_buff_state,client_sock);
        //  Error reading
        if (readret == -1) {
            //close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Lisod: error reading from client socket.\n");
        }  

    }

return NULL;
}

void init_pool(int listenfd, pool *p) {
  
  int i;
  p->maxi = -1;
  for (i=0; i< FD_SETSIZE; i++) {            /* Initially, there are no active descriptors */
    p->clientfd[i] = -1;
  }
  p->maxfd = listenfd;                       /* Initially, listenfd is the only member of select read set */
  FD_ZERO(&p->master_sockets);
  FD_SET(listenfd, &p->master_sockets);
}

int main(int argc, char* argv[])
{
    struct sockaddr_in addr; // cli_addr;
    static pool pool;

    fprintf(stdout, "----- Echo Server -----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);

    addr.sin_family = AF_INET; // AF_INET and PF_INET are closely related, they have the same value
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    //FD_ZERO(&master_sockets);  // creats an empty read set
    //FD_ZERO(&ready_sockets);

    //FD_SET(sock, &master_sockets); // add sock descriptor to read set
    //maxfd = sock;

    fprintf(stdout,"listener is the welcome socket of id %d\n",sock);
    
    init_pool(sock, &pool);                                     /* Initialize the pool */

    /* waiting for a new client connection or data to read from an exist client connection */
    while (1) 
    {
        //ready_sockets = master_sockets; 
        pool.ready_sockets = pool.master_sockets;                           /* Wait for listen/connected fd to be ready */

        /* server's process waits until it finds a new thing ready to read it 
        (i.e., blocks until at least one descriptor in the read set is ready for reading) */
        if ((pool.nready = select(pool.maxfd+1, &pool.ready_sockets, NULL, NULL, NULL)) == -1) { 
            fprintf(stderr, "Lisod: selecting error.\n");
            return EXIT_FAILURE;
        }

        printf("\n");
        fprintf(stdout,"Lisod: running %d file desriptors (maxfd).\n",pool.maxfd); 
        /* check existing connections looking for data to read */

        if (FD_ISSET(sock, &pool.ready_sockets))
            handle_new_client_connection(sock, &pool);  /* handle new connections */
        else
        {
            manage_client_request(sock, &pool);
            //handle_data_from_client(sock, &pool);   /* handle data from a client */

        }
    //    for(int i = 0; i <= pool.maxfd; i++) {
            
    //        if (FD_ISSET(i, &pool.ready_sockets)) {           /*return true if socket (or fd) i is in the read's ready set */
                
    //            if (i == sock) {                        /* if the fd is the same as the welcome socket */

    //                handle_new_client_connection(sock, &pool);  /* handle new connections */

    //           } else {                                 /* it means that fd belengs to one the connected clients */  

    //                handle_data_from_client(sock, i, &pool);   /* handle data from a client */
    //            } 
    //        } 
    //    } 

    }
    return EXIT_SUCCESS;
} 

