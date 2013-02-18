/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
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
#include <map>
#include <string.h>

using namespace std;

const string SERVER_PORT = "14891";
const size_t BUFSIZE = 1024;
const int MAXNUMPROC = 10;
map<string,HttpResponse> PageCache;

int GetHeaderStats(char* buf,int size, char** headerStart)
{
  *headerStart = ((char *)memmem (buf, size, "\r\n", 2))+2;
  
  for(int i = 0; *(buf+i) != '\r' && *(buf+i+1) != '\n'; i++)
    {
      size--;
    }
  return size - 2;
} 

int ReceiveHttpHeader(int sock_d, char* buf, bool isresponse)
{
  time_t start, end;
  int numbytes = 0;
  int totalbytes = 0;
  char* hbuf = NULL;
  int headersize = 0;
  int datasize = 0;
  int dataread = 0;
  HttpHeaders head;
  
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
      //return -2;
      //return 1;
      //cout << numbytes << endl;
      if(difftime (end,start) > 15.0)
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
  if(isresponse)
  {
    headersize = GetHeaderStats(buf,totalbytes,&hbuf);
    head.ParseHeaders(hbuf,headersize);
    datasize = atoi(head.FindHeader("Content-Length").c_str());
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
        if(difftime (end,start) > 15.0)
        {
          cout << "Connection timed out" << endl;
          cout << "Connection: " << sock_d << " closed." << endl << endl;
          close(sock_d);
          return -2;
        }
        time(&end);
      }
      //Track total bytes read
      dataread += numbytes;
      totalbytes += numbytes;
      
      //If 0 bytes are read the client has closed the connection
      if(numbytes == 0)
      {
        cout << "Connection: " << sock_d << " closed." << endl << endl;
        close(sock_d);
        return totalbytes;
      }
      
      //If the last 4 bytes match the pattern '\r\n\r\n' then we have a complete request
      if(dataread == datasize)
      {
          break;
      }
     
    }
  }
  return totalbytes;
  
}


int HandleClient(int sock_client)
{
  int sock_server;
  int numbytes;
  int datasize;
  int headersize;
  int totalbytes = 0;
  char cbuf[BUFSIZE];
  char sbuf[BUFSIZE];
  //char* dbuf = NULL; 
  char* hbuf = NULL;
  stringstream ss,test;
  size_t tempBufSize;
  char* tempBuf;
  struct addrinfo hostai, *result;
  
  
  while(1)
  {
    totalbytes = ReceiveHttpHeader(sock_client,cbuf,false);
    cout << "Recieve status: " << totalbytes << endl;
    if(totalbytes == 0)
      return 0;
  
    HttpRequest req;
    HttpResponse rep;
    HttpHeaders head;
    
    try
    {
      req.ParseRequest(cbuf,totalbytes);
    }
    catch (ParseException ex)
    {
      string cmp = "Request is not GET";
      if (strcmp(ex.what(), cmp.c_str())){
      rep.SetStatusCode("501");
      rep.SetStatusMsg("Not Implemented");
      rep.SetVersion(req.GetVersion());      	
      }
      else {
      rep.SetStatusCode("400");
      rep.SetStatusMsg("Bad Request");
      rep.SetVersion(req.GetVersion());
      }
      
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
      rep.FormatResponse(tempBuf);
      cout << "Sent " << send(sock_client, tempBuf, tempBufSize,0) << " bytes" << endl;
      free(tempBuf);
      return 0;
    } 
    
    string version = req.GetVersion();
    if( version == "1.0")
      req.ModifyHeader("Connection", "close");
    /*const char *endline = (const char *)memmem (cbuf, totalbytes, "\r\n", 2);
    
    int iter = 0;
    int totalheaderbytes = totalbytes-2;
    while (*(cbuf+iter) != '\r' && *(cbuf+iter+1) != '\n')
      {
        iter++;
        totalheaderbytes--;
      }
    
    cout << *(endline) << endl;
    cout << *(endline+1) << endl;
    cout << *(endline+2) << endl;

    head.ParseHeaders(endline+2,totalheaderbytes);
    */
    head.ParseHeaders(cbuf,totalbytes);
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
    //Check to see if it's in there and ...
    
    /* Design for proxy server cache.  For each page that we fetch, we store the message.  That can be done via a map that maps each ip and path to a message.  Then, we search the map for the ip and path before we create a new socket connection and we create a response based on our stored message rather than forwarding the new request.  Only question is what to do with the headers.*/
    bool cached = false;
    string HostPath = req.GetHost()+req.GetPath();
    for(map<string,HttpResponse>::iterator ii=PageCache.begin(); ii!=PageCache.end(); ++ii){
    if ( HostPath== (*ii).first ){
    	//this means the object was found in the cache, return the HttpResponse via socket.
      cached = true;
      
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
  
      (*ii).second.FormatResponse(tempBuf);
  
  
      cout << "Sent " << send(sock_client, tempBuf, tempBufSize,0) << " bytes header" << endl;
      cout << "Sent " << send(sock_client, sbuf+tempBufSize, datasize,0) << " bytes data" << endl;
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
    }
    	
    	
    	
    	
    

    if (!cached) 
    {
    	sock_server = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
    	connect(sock_server,result->ai_addr, result->ai_addrlen);
    
    	//Allocate space to build the HTTP request
    	tempBufSize = req.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
    
    	req.FormatRequest(tempBuf);    
  
      cout << "Sent " << send(sock_server,tempBuf,tempBufSize,0) << " bytes to server" << endl;
  
      free(tempBuf);
      //cout << "??" << endl;
      numbytes = ReceiveHttpHeader(sock_server,sbuf,true);
      cout << "Recv " << numbytes << " from server." << endl;
 
      headersize = GetHeaderStats(sbuf,numbytes,&hbuf);
      head.ParseHeaders(hbuf,headersize);
      datasize = atoi(head.FindHeader("Content-Length").c_str());

      close(sock_server);
      rep.ParseResponse(sbuf,numbytes);

      ///
      PageCache.insert(map<string,HttpResponse>::value_type(HostPath,rep));
      ///
      ss.str("");
  
      ss << "Version: " << rep.GetVersion() << endl;
      ss << "Status Code: " << rep.GetStatusCode() << endl;
      ss << "Status Message: " << rep.GetStatusMsg() << endl;
      cout << ss.str();
  
  
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
  
      rep.FormatResponse(tempBuf);
  
  
      cout << "Sent " << send(sock_client, tempBuf, tempBufSize,0) << " bytes header" << endl;
      cout << "Sent " << send(sock_client, sbuf+tempBufSize, datasize,0) << " bytes data" << endl;
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
    else {  //Cached, get it from internal memory and send it.
      
      
      
      
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

	status = getaddrinfo(NULL, SERVER_PORT.c_str(), &hostai, &result); //take the flags set in hostai and create an addrinfo with those values and flags stuff made.
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
  int procCount = 0;
  while(1)
  {
    addr_size = sizeof their_addr;

    sock_new = accept(sock_d, (struct sockaddr *)&their_addr, &addr_size);  //accept the connection
    int wpstat = waitpid(-1,NULL,WNOHANG);
    if(procCount > 0 && wpstat > 0)
    {
      cout << "WP Stat " << wpstat << endl;
      procCount--;
    }
    procCount++;
    cout << "Number of procs " << procCount << endl;
    if (procCount > MAXNUMPROC)
    {
      int tempBufSize;
      char* tempBuf;
      //Error Server busy/full
      cout << "==========================" << endl;
      cout << "Server Full" << endl;
      cout << "==========================" << endl;
      HttpResponse rep;
      HttpHeaders head;
      rep.SetStatusCode("503");
      rep.SetStatusMsg("Service Unavailable");
      rep.SetVersion("1.1");
      head.AddHeader("Connection","close");
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
      rep.FormatResponse(tempBuf);
      cout << "Sent " << send(sock_new, tempBuf, tempBufSize,0) << " bytes" << endl;
      close(sock_new);
      free(tempBuf);
      procCount--;
    }
    else if(sock_new)
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
