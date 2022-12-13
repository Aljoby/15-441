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

#define ECHO_PORT 9999 /* argv[1] is used instead */
#define BUF_SIZE 4096


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
fd_set master_sockets;     /* current clients socket descriptors and welcome socket descriptor */
fd_set ready_sockets;     /* temp socket descriptor list for select() */
int maxfd;               /* max number of descriptors in the system */

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
void allocate_new_client(int client_sock){

    struct client_state *temp_client_state = (struct client_state*) calloc (1, sizeof(struct client_state));
    

    if (!temp_client_state) {
        fprintf(stderr, "Error allocation for a new client.\n");
        exit(1);
    }

    temp_client_state->client_socket_fd = client_sock;
    temp_client_state->next = clients_buff_state;
    clients_buff_state = temp_client_state;

    FD_SET(client_sock, &master_sockets);   /* add the client to master_sockets set */
    if (client_sock > maxfd) {             /* keeping track of the maxfd */
        maxfd = client_sock;
    }
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

    struct client_state *cli_buff_state = get_client_state(client_sock);

    FD_CLR(cli_buff_state->client_socket_fd, &master_sockets); // remove from master_sockets set
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


int manage_client_request(int client_sock){

struct client_state *cli_buff_state = get_client_state(client_sock);
    // receive data
    int clientdata = 0;
    clientdata = recv(cli_buff_state->client_socket_fd, cli_buff_state->req_buf, BUF_SIZE,0);
    
    if (clientdata > 0){        
       
        fprintf(stdout, "Lisod: %d bytes received from client %d\n", clientdata, client_sock);

        if (sendall(cli_buff_state->client_socket_fd, cli_buff_state->req_buf, &clientdata) == -1) { // req_buf is the base address for received bytes 
            fprintf(stderr, "Lisod: error sending.\n");
            fprintf(stdout, "Lisod: we only sent %d bytes!\n", clientdata);
        }

        //fprintf(stdout, "Lisod: received request from client %d is :%s of size %d.\n", client_sock, cli_buff_state->req_buf, clientdata);
    }
    return clientdata;
}


/*arrival of new client */
void *handle_new_client_connection(int sock){
    
    int client_sock; 
    struct sockaddr_in cli_addr; /* client address */
    socklen_t cli_size = sizeof(cli_addr);
    char clientIP[INET6_ADDRSTRLEN];

    if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size)) == -1){
        close(sock);
        fprintf(stderr, "Lisod: error accepting a new connection.\n");
        return NULL;
    } else if (client_sock < FD_SETSIZE) { /* check if the newely accepted client socket fd is less than allowed max number of fds for the Lisod */
        
        allocate_new_client(client_sock);
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

void *handle_data_from_client(int sock, int client_sock){

    int readret = 0;
    readret = manage_client_request(client_sock);
    if (readret >= BUF_SIZE){
        fprintf(stderr, "client data higher than its allocated buffer.\n");
        deallocate_a_client(&clients_buff_state,client_sock);
    } else if (readret > 1 && readret < BUF_SIZE){

        //fprintf(stdout, "Lisod: nBytes has sent %d\n",readret);

    }else if (readret < 1 ){
        
        deallocate_a_client(&clients_buff_state,client_sock);
        //  Error reading
        if (readret == -1) {
            //close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Lisod: error reading from client socket.\n");
        }  

    }

return NULL;
}


int main(int argc, char* argv[])
{
    struct sockaddr_in addr; //cli_addr;

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

    FD_ZERO(&master_sockets);  
    FD_ZERO(&ready_sockets);

    FD_SET(sock, &master_sockets);
    maxfd = sock;

    fprintf(stdout,"listener is the welcome socket of id %d\n",sock);

    /* waiting for a new client connection or data to read from an exist client connection */
    while (1) 
    {
        ready_sockets = master_sockets; 

        /* server's process waits until it finds a new thing ready to read it */
        if (select(maxfd+1, &ready_sockets, NULL, NULL, NULL) == -1) { 
            fprintf(stderr, "Lisod: selecting error.\n");
            return EXIT_FAILURE;
        }

        printf("\n");
        fprintf(stdout,"Lisod: running %d file desriptors (maxfd).\n",maxfd); 
        /* check existing connections looking for data to read */

        for(int i = 0; i <= maxfd; i++) {
            
            if (FD_ISSET(i, &ready_sockets)) {           /*return true if socket (or fd) i is in the read's ready set */
                
                if (i == sock) {                        /* if the fd is the same as the welcome socket */

                    handle_new_client_connection(sock);  /* handle new connections */

                } else {                                 /* it means that fd belengs to one the connected clients */  

                    handle_data_from_client(sock, i);   /* handle data from a client */
                } 
            } 
        } 
    }
    return EXIT_SUCCESS;
} 