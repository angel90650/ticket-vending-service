 /* Movie Server */
#include <stdio.h>
#include<sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <netdb.h>

#define CAPACITY 1024
#define MAX_KIOSKS 5

int kiosk_num = 0;
int tickets_left = 50;
int socket_fd, play_fd;
struct sockaddr_in play_server;
sem_t lock;
struct hostent* host;

struct client_message{
  int fd;
  char buf[CAPACITY];
  int * kiosk_ptr;
};

void *kiosk_handler(void *ptr);

int main(int argc, char ** argv){

  int client_fd, child_pid, return_val;
  char msg_type;
  char buffer[CAPACITY];
  socklen_t client_length;
  struct sockaddr_in movie_server, client_addr;

  sem_init(&lock, 0, 1);


  if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("Could not open socket");
    exit(EXIT_FAILURE);
  }

  movie_server.sin_family = AF_INET;
  movie_server.sin_addr.s_addr = htonl(INADDR_ANY);
  movie_server.sin_port = htons(atoi(argv[1]));

  host = gethostbyname(argv[3]);
  bzero((char *) &play_server, sizeof(play_server));
  play_server.sin_family = AF_INET;
  bcopy((char *)host->h_addr, 
	  (char *)&play_server.sin_addr.s_addr, host->h_length);
  play_server.sin_port = htons(atoi(argv[2]));

  if((return_val = bind(socket_fd, (struct sockaddr *) &movie_server, sizeof(movie_server))) < 0){
    perror("Could not bind socket");
    exit(EXIT_FAILURE);
  }

  if((return_val = listen(socket_fd, 2*MAX_KIOSKS)) < 0){
    perror("Listen Failed");
    exit(EXIT_FAILURE);
  }

  
  while(1){
    client_length = sizeof(client_addr);
    if((client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_length)) < 0){
      perror("Connection acceptance failed");
      exit(EXIT_FAILURE);
    }

    struct client_message * message;
    message = (struct client_message *)malloc(sizeof(struct client_message));
    message->fd = client_fd;
    message->kiosk_ptr = &kiosk_num;

    pthread_t id;
    return_val = pthread_create(&id, NULL, kiosk_handler, (void *)message);
    if(return_val != 0){
      perror("Thread create failed");
      exit(EXIT_FAILURE);
    }
  }
}

void *kiosk_handler(void *ptr){
  struct client_message * msg = (struct client_message *)ptr;
  char msg_type, ticket_type, msg_response;
  int return_val, num_tickets, kiosk_id;
  int a;
  while(1){
    if((return_val = recv(msg->fd, msg->buf, CAPACITY, 0)) < 0){
      perror("Receive Failed");
      exit(EXIT_FAILURE);
    }

    if(return_val  == 0){
      printf("Kiosk %d has closed\n", kiosk_id);
      break;
    }

    msg_type = msg->buf[0];

    switch(msg_type) {
    case 'c' :
      (*(msg->kiosk_ptr))++;
      bzero(msg->buf, CAPACITY);
      sprintf(msg->buf, "%d", *msg->kiosk_ptr);
      if((return_val = send(msg->fd, msg->buf, 2, 0)) < 0){
	perror("Send failed");
	exit(EXIT_FAILURE);
      }
      kiosk_id = *msg->kiosk_ptr;
      break;
    case 'm' :
      sscanf(msg->buf, "%c %d %d", &ticket_type, &num_tickets, &kiosk_id);
      printf("Received request, ticket type: %c, Number of tickets: %d, Kiosk ID: %d\n", ticket_type, num_tickets, kiosk_id);
      if(tickets_left < num_tickets){
	msg_response = 'n';
	printf("Could not complete request\n");
      }else{
	sem_wait(&lock);
	tickets_left -= num_tickets;
	sem_post(&lock);
	msg_response = 'y';
	printf("Request completed\n");
	printf("Tickets remaining: %d\n", tickets_left);
      }
      bzero(msg->buf, CAPACITY);
      sprintf(msg->buf, "%c", msg_response);
      printf("Sending message to kiosk %d\n", kiosk_id);
      if((return_val = send(msg->fd, msg->buf, 2, 0)) < 0){
	perror("Send failed");
	exit(EXIT_FAILURE);
      }
      break;
     case 'p' :
       if((play_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	 perror("Could not open socket");
	 exit(EXIT_FAILURE);
       }
      if((return_val = connect(play_fd, (struct sockaddr *) &play_server, sizeof(play_server))) < 0){
	perror("Connection failed");
	exit(EXIT_FAILURE);
      }
      sscanf(msg->buf, "%c %d %d", &ticket_type, &num_tickets, &kiosk_id);
      ticket_type = 's';
      bzero(msg->buf, CAPACITY);
      sprintf(msg->buf, "%c %d %d", ticket_type, num_tickets, kiosk_id);
      printf("Fowarding request to other server\n");
      if((return_val = send(play_fd, msg->buf, CAPACITY, 0)) < 0){
	perror("Send failed");
	exit(EXIT_FAILURE);
      }
      if((return_val = recv(play_fd, msg->buf, CAPACITY, 0)) ==  0){
	perror("Received failed");
	exit(EXIT_FAILURE);
       }
      printf("Received response from other server\n");
       msg_response = msg->buf[0];
       if(msg_response == 'y'){
	 printf("purchase successful\n");
       }else if(msg_response == 'n'){
	 printf("purchase unsuccessful\n");
       }else{
	 printf("Unexpected message\n");
       }
       bzero(msg->buf, CAPACITY);
       sprintf(msg->buf, "%c", msg_response);
       printf("Forwarding response to kiosk %d\n", kiosk_id);
       if((return_val = send(msg->fd, msg->buf, 2, 0)) < 0){
	perror("Send failed");
	exit(EXIT_FAILURE);
       }
       break;
    case 's' :
      sscanf(msg->buf, "%c %d %d", &ticket_type, &num_tickets, &kiosk_id);
      printf("Request from other server, ticket type: %c, Number of tickets: %d, Kiosk ID: %d\n", ticket_type, num_tickets, kiosk_id);
      if(tickets_left < num_tickets){
	msg_response = 'n';
	printf("Could not complete request\n");
      }else{
	sem_wait(&lock);
	tickets_left -= num_tickets;
	sem_post(&lock);
	msg_response = 'y';
	printf("Request completed\n");
	printf("Tickets remaining: %d\n", tickets_left);
      }
      bzero(msg->buf, CAPACITY);
      sprintf(msg->buf, "%c", msg_response);
      printf("Sending message to other server\n");
      if((return_val = send(msg->fd, msg->buf, 2, 0)) < 0){
	perror("Send failed");
	exit(EXIT_FAILURE);
      }
      close(msg->fd);
      pthread_exit(NULL);
      break;

    }
  }
  close(msg->fd);
}
