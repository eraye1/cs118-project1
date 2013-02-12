/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "http-request.h"
#include "http-response.h"
#include "http-headers.h"
#include <sstream>
#include <fcntl.h>
#include <time.h>


const size_t BUFSIZE = 1024;

using namespace std;

int HandleClient(int sock_d)
{
  //bool newline = false;
  int numbytes;
  int totalbytes = 0;
  char buf[BUFSIZE];
  stringstream ss,test;
  size_t responseSize;
  time_t start, end;
  
  fcntl(sock_d, F_SETFL, O_NONBLOCK);
  
  while(1)
  { 
    while(1)
    {
      time(&start);
      time(&end);
      while((numbytes = recv(sock_d, buf+totalbytes, BUFSIZE, 0)) == -1) {
        //cout << "Error: recv" << endl;
        //return 1;
        //cout << numbytes << endl;
        if(difftime (end,start) > 10.0)
        {
          close(sock_d);
          return 0;
        }
        time(&end);
        continue;
      }
      totalbytes += numbytes;
      if(numbytes == 0)
      {
        cout << "Connection: " << sock_d << " closed." << endl;
        close(sock_d);
        return 0;
      }
      if(numbytes == 2)
      {
        buf[totalbytes-2] = '\r';
        buf[totalbytes-1] = '\n';
        //if(newline)
          break;
        //newline = true;
      }
      //else
      //{
        //newline = false;
      //}
    }
    //buf[totalbytes] = '\0';
    //cout << buf << endl;
    HttpRequest req;
    HttpResponse rep;
    HttpHeaders head;
    
    req.ParseRequest(buf,totalbytes);
    //head.ParseHeaders(buf,totalbytes);
    
    ss << "Method: " << req.GetMethod() << endl;
    ss << "Host: " << req.GetHost() << endl;
    ss << "Port: " << req.GetPort() << endl;
    ss << "Path: " << req.GetPath() << endl;
    ss << "Version: " << req.GetVersion() << endl;
    
    cout << ss.str();
    
    rep.SetVersion("1.1");
    rep.SetStatusMsg("Bad Request");
    rep.SetStatusCode("404");
    
    responseSize = rep.GetTotalLength();
    char* tempBuf = (char*)malloc(responseSize);
    
    rep.FormatResponse(tempBuf);
    
    
    cout << "Sent " << send(sock_d, tempBuf, responseSize,0) << " bytes" << endl;
    //if(head.FindHeader("") == "")
    //{
    //  close(sock_d);
    // return 0;
    //}
    
    //cout << "Sent " << send(sock_d, "\r\n", 2,0) << " bytes" << endl;
    //buf[numbytes] = '\0';
    //cout << buf ;
    //if (send(sock_d,buf, numbytes, 0) == -1)
    //{
      //cout << "Error: send" << endl;
      //return 1;
    //}
    
    
  }
  
}



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
  int sock_new;
  struct addrinfo hostai, *result;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  
  memset(&hostai,0,sizeof hostai);
  hostai.ai_family = AF_INET;  //IPv4, but not sure if we need to do IPv6? No
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
  
  while(1)
  {
    addr_size = sizeof their_addr;
    sock_new = accept(sock_d, (struct sockaddr *)&their_addr, &addr_size);  //accept the connection
    if(sock_new)
    {
      if (!fork()) // this is the child process 
      {
        close(sock_d);  // child doesn't need the listener
        HandleClient(sock_new);
        return 0;
      }
      cout << "Connection: " << sock_new << " opened." << endl;
      close(sock_new); //Getting "Address alread in use" error immediatly after a run... thought this would release the socket and fix it.
    }
  }
  
  
  
  return 0;
}
