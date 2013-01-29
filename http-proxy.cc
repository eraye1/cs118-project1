/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

using namespace std;

int main (int argc, char *argv[])
{
  //http-proxy called without command-line parameters
  if (argc != 1)
    {
      cout << "Cannot include parameters with http-proxy call\n";
      return 1;
    }


  //making sockets like we're sock-puppets
  int sock_d;
  struct addrinfo hostai, *result;
  struct sockaddr_storage their_addr;
  
  memset(&result,0,sizeof result);
  hostai.ai_family = AF_INET;  //IPv4, but not sure if we need to do IPv6?
  hostai.ai_socktype = SOCK_STREAM;  //stream socket, uses TCP
  hostai.ai_flags = AI_PASSIVE;  //fill in IP automatically, useful for binding

  getaddrinfo(NULL, "14805", &hostai, &result);  //take the flags set in hostai and create an addrinfo with those values and flags stuff made.
  
  sock_d = socket(result->ai_family,result->ai_socktype,0);  //create socket with hostai properties, 0 for the protocol field automatically chooses the correct protocol
  bind(sock_d, result->ai_addr, result-> ai_addrlen); //bind socket to result addrinfo

  int listenresult = listen(sock_d,15);  //silently limits incoming queue to 15

  //print the sd because warnings treated as errors
  cout << "socketfd: " << sock_d << "\nlistenresult: " << listenresult;

  socklen_t addr_size = sizeof their_addr;
  int acceptres = accept(sock_d, (struct sockaddr *)&their_addr, &addr_size);  //accept the connection
  cout <<  "\nacceptres: " << acceptres << "\n";  
  
  return 0;
}
