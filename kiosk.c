/* kiosk */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#define CAPACITY 1024
#define MOVIE_PORT 9003
#define PLAY_PORT 5001

int main(int argc, char ** argv){

  int movie_fd, play_fd, return_val, num_tickets, kiosk_id, server_fd;
  struct sockaddr_in movie_server, play_server;
  char buffer[CAPACITY];
  struct hostent* host;
  char ticket_type, msg_response;
  bzero(buffer, CAPACITY);
  if((movie_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("Could not open socket");
    exit(EXIT_FAILURE);
  }
  if((play_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("Could not open socket");
    exit(EXIT_FAILURE);
  }

  host = gethostbyname(argv[3]);
  bzero((char *) &movie_server, sizeof(movie_server));
  movie_server.sin_family = AF_INET;
  bcopy((char *)host->h_addr, 
	  (char *)&movie_server.sin_addr.s_addr, host->h_length);
  movie_server.sin_port = htons(atoi(argv[1]));

  host = gethostbyname(argv[4]);
  bzero((char *) &play_server, sizeof(play_server));
  play_server.sin_family = AF_INET;
  bcopy((char *)host->h_addr, 
	  (char *)&play_server.sin_addr.s_addr, host->h_length);
  play_server.sin_port = htons(atoi(argv[2]));

  if((return_val = connect(movie_fd, (struct sockaddr *) &movie_server, sizeof(movie_server))) < 0){
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }
  if((return_val = connect(play_fd, (struct sockaddr *) &play_server, sizeof(play_server))) < 0){
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }
  if((return_val = send(movie_fd, "c", 2, 0)) < 0){
    perror("Send failed");
    exit(EXIT_FAILURE);
  }
  if((return_val = recv(movie_fd, buffer, CAPACITY, 0)) ==  0){
    perror("Received failed");
    exit(EXIT_FAILURE);
  }
  if((kiosk_id = atoi(buffer)) != 0){
    printf("kiosk id: %d \n", kiosk_id);
  }else{
    printf("Invalid ID\n");
    exit(EXIT_FAILURE);
  }
  bzero(buffer, CAPACITY);
  sprintf(buffer, "b %d", kiosk_id);
  if((return_val = send(play_fd, buffer, CAPACITY, 0)) < 0){
    perror("Send failed");
    exit(EXIT_FAILURE);
  }
  
  while(1){
    bzero(buffer, CAPACITY);
    while(1){
      printf("Would you like to purchase a movie(m) or play(p) ticket?:\n");
      if (fgets(buffer, sizeof(buffer), stdin)) {
	if (1 == sscanf(buffer, "%c", &ticket_type)) {
	}
      }
      if(ticket_type == 'm' || ticket_type == 'p'){
	break;
      }else{
	printf("Invalid input\n");
      }
    }

    while(1){
      printf("How many would you like to purchase?:\n");
      fgets(buffer, CAPACITY, stdin);
      num_tickets = atoi(buffer);
      if(num_tickets > 0){
	break;
      }else{
	printf("Invalid Input\n");
      }
    }
    if((rand() % 10) >= 5){
      server_fd = movie_fd;
    }else{
      server_fd = play_fd;
    }
    printf("Processing your request of %d tickets of type:%c \n", num_tickets, ticket_type);
    // Fill buffer with request to send
    bzero(buffer, CAPACITY);
    sprintf(buffer, "%c %d %d", ticket_type, num_tickets, kiosk_id);
    printf("Kiosk %d sending request for %d tickets of type %c \n", kiosk_id, num_tickets, ticket_type);
    if((return_val = send(server_fd, buffer, CAPACITY, 0)) < 0){
      perror("Sending request failed");
      exit(EXIT_FAILURE);
    }

    bzero(buffer, CAPACITY);
    if((return_val = recv(server_fd, buffer, CAPACITY, 0)) ==  0){
      perror("Received failed");
      exit(EXIT_FAILURE);
    }
    printf("Kiosk %d received response\n", kiosk_id);
    msg_response = buffer[0];
    if(msg_response == 'y'){
      printf("purchase successful\n");
    }else if(msg_response == 'n'){
      printf("purchase unsuccessful\n");
    }else{
      printf("Unexpected message\n");
    }
  }
}
