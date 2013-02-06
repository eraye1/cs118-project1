/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

using namespace std;

int main (int argc, char *argv[])
{
  //http-proxy called without command-line parameters
  if (argc != 1)
    {
      cout << "Cannot include parameters with http-proxy call\n";
      return 1;
    }

	int status;
  //making sockets like we're sock-puppets
  int sock_d;
  int acceptres;
  struct addrinfo hostai, *result;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  
  memset(&hostai,0,sizeof hostai);
  hostai.ai_family = AF_INET;  //IPv4, but not sure if we need to do IPv6?
  hostai.ai_socktype = SOCK_STREAM;  //stream socket, uses TCP
  hostai.ai_flags = AI_PASSIVE;  //fill in IP automatically, useful for binding

	status = getaddrinfo(NULL, "14891", &hostai, &result); //take the flags set in hostai and create an addrinfo with those values and flags stuff made.
  if(status) 
	{
		cout << "Error: getaddrinfo() returned " << status << endl;
		return 1;
	} 
  
  sock_d = socket(result->ai_family,result->ai_socktype,result->ai_protocol);  //create socket with hostai properties, 0 for the protocol field automatically chooses the correct protocol
  if(sock_d == -1)
  {
		cout << "Error: socket() failed returned " << sock_d;
		return 2;
  }
  
  status = bind(sock_d, result->ai_addr, result-> ai_addrlen); //bind socket to result addrinfo
	if(status) 
	{
		cout << "Error: bind() returned " << status << endl;
    cout << "Errno: " << strerror (errno) << endl;
		return 3;
	} 

  int listenresult = listen(sock_d,15);  //silently limits incoming queue to 15

  //print the sd because warnings treated as errors
  cout << "socketfd: " << sock_d << endl;
  cout << "listenresult: " << listenresult << endl;
  //Broke these lines up and replaced '\n' with endl and it stopped 
  //getting stuck and accepted my telnet requests.
  
  
	addr_size = sizeof their_addr;
  acceptres = accept(sock_d, (struct sockaddr *)&their_addr, &addr_size);  //accept the connection
  cout <<  endl << "acceptres: " << acceptres << endl;  
  close(sock_d); //Getting "Address alread in use" error immediatly after a run... thought this would release the socket and fix it.
  
  
  return 0;
}
