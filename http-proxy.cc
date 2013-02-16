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

int ReceiveHttpData(int sock_d, char* buf)
{
  time_t start, end;
  int numbytes = 0;
  int totalbytes = 0;
  
  
  fcntl(sock_d, F_SETFL, O_NONBLOCK);
  
  while(1)
  {
    //Will be used to track connection that has timed out
    time(&start);
    time(&end);
    
    //Polling read
    while((numbytes = recv(sock_d, buf+totalbytes, BUFSIZE, 0)) == -1) 
    {
      //cout << "Error: recv" << endl;
      //return 1;
      //cout << numbytes << endl;
      if(difftime (end,start) > 10.0)
      {
        cout << "Connection timed out" << endl;
        cout << "Connection: " << sock_d << " closed." << endl << endl;
        close(sock_d);
        return -2;
      }
      time(&end);
    }
    //Track total bytes read
    totalbytes += numbytes;
    
    //If 0 bytes are read the client has closed the connection
    if(numbytes == 0)
    {
      cout << "Connection: " << sock_d << " closed." << endl << endl;
      close(sock_d);
      return 0;
    }
    
    //If the last 4 bytes match the pattern '\r\n\r\n' then we have a complete request
    if(totalbytes > 4 && buf[totalbytes-4] == '\r' && buf[totalbytes-3] == '\n' && buf[totalbytes-2] == '\r' && buf[totalbytes-1] == '\n')
    {
        break;
    }
   
  }
  
  return totalbytes;
  
}


int HandleClient(int sock_client)
{
  int sock_server;
  int numbytes;
  int totalbytes = 0;
  char cbuf[BUFSIZE];
  char sbuf[BUFSIZE];
  stringstream ss,test;
  size_t tempBufSize;
  char* tempBuf;
  struct addrinfo hostai, *result;
  
  
  while(1)
  {
    totalbytes = ReceiveHttpData(sock_client,cbuf);
    cout << "Recieve status: " << totalbytes << endl;
    if(totalbytes == 0)
      return 0;
  
    HttpRequest req;
    HttpResponse rep;
    HttpHeaders head;
    
    req.ParseRequest(cbuf,totalbytes);
    const char *endline = (const char *)memmem (cbuf, totalbytes, "\r\n", 2);
    head.ParseHeaders(endline+4,totalbytes-(cbuf-endline));
    //TODO
    //Need to check all the headers(expiration, etc) not just Connection: close
    //Same goes for information recieved from server further down.
    
    ss << "Method: " << req.GetMethod() << endl;
    ss << "Host: " << req.GetHost() << endl;
    ss << "Port: " << req.GetPort() << endl;
    ss << "Path: " << req.GetPath() << endl;
    ss << "Version: " << req.GetVersion() << endl;
    
    cout << ss.str();
    
    memset(&hostai,0,sizeof hostai);
    hostai.ai_family = AF_INET;  //IPv4, but not sure if we need to do IPv6? No
    hostai.ai_socktype = SOCK_STREAM;  //stream socket, uses TCP
    hostai.ai_flags = AI_PASSIVE;  //fill in IP automatically, useful for binding
    
    ss.str("");
    ss << req.GetPort();
    
    //Connect to requested Server
    getaddrinfo(req.GetHost().c_str(), ss.str().c_str(), &hostai, &result); //take the flags set in hostai and create an addrinfo with those values and flags stuff made.
    //TODO Implement caching hereish?  This is where we can map ip and path to data.
    sock_server = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
    connect(sock_server,result->ai_addr, result->ai_addrlen);
    
    //Allocate space to build the HTTP request
    tempBufSize = req.GetTotalLength();
    tempBuf = (char*)malloc(tempBufSize);
    
    req.FormatRequest(tempBuf);    
    
    cout << "Sent " << send(sock_server,tempBuf,tempBufSize,0) << " bytes to sever" << endl;
    
    free(tempBuf);
    //cout << "??" << endl;
    numbytes = ReceiveHttpData(sock_server,sbuf);
    cout << "Recv " << numbytes << " from server." << endl;
    //TODO
    //Currently we only get the header from the server.
    //Need to call ReceiveHttpData() again with a new buffer to get page data

    close(sock_server);
    rep.ParseResponse(sbuf,numbytes);
    
    ss.str("");
    
    ss << "Version: " << rep.GetVersion() << endl;
    ss << "Status Code: " << rep.GetStatusCode() << endl;
    ss << "Status Message: " << rep.GetStatusMsg() << endl;
    cout << ss.str();
    
    
    tempBufSize = rep.GetTotalLength();
    tempBuf = (char*)malloc(tempBufSize);
    
    rep.FormatResponse(tempBuf);
    
    
    cout << "Sent " << send(sock_client, tempBuf, tempBufSize,0) << " bytes" << endl;
    free(tempBuf);
    if(head.FindHeader("Connection") == "close")
    {
      cout << "HEADER Connection: close found" << endl;
      cout << "Connection: " << sock_client << " closed." << endl << endl;
      close(sock_server);
      close(sock_client);
      return 0;
    }
  }
  
  return 0;
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
