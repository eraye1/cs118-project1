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
#include <pthread.h>



using namespace std;

<<<<<<< HEAD
pthread_mutex_t read_mutex;
=======

>>>>>>> 1d595f70174d7267adb0461695da03c8b137d765
pthread_mutex_t cache_mutex;
pthread_mutex_t tcount_mutex;
const string SERVER_PORT = "14891";
const size_t BUFSIZE = 1024;
const int MAXNUMTHREAD = 10;
map<string,string> PageCache;
int threadCount = 0;

int GetHeaderStats(const char* buf,int size, char** headerStart)
{
  *headerStart = ((char *)memmem (buf, size, "\r\n", 2))+2;
  
  for(int i = 0; *(buf+i) != '\r' && *(buf+i+1) != '\n'; i++)
    {
      size--;
    }
  return size - 2;
} 

bool isCached(string url)
{ 
  map<string,string>::iterator it;
  pthread_mutex_lock(&cache_mutex);
  it = PageCache.find(url);
  if(it == PageCache.end())
  {
    pthread_mutex_unlock(&cache_mutex);
    return false;
  }
  pthread_mutex_unlock(&cache_mutex);
  return true;
}

int ReceiveHttpData(int sock_d,string& data, bool isresponse)
{
  char buf[BUFSIZE];
  time_t start, end;
  int numbytes = 0;
  int totalbytes = 0;
  char* hbuf = NULL;
  int headersize = 0;
  int datasize = 0;
  int dataread = 0;
  HttpHeaders head;
  HttpResponse rep;
  
  fcntl(sock_d, F_SETFL, O_NONBLOCK);
  
  while(1)
  {
    //Will be used to track connection that has timed out
    time(&start);
    time(&end);
    
    //Polling read
    while((numbytes = recv(sock_d, buf, BUFSIZE, 0)) == -1) 
    {

      if(difftime (end,start) > 15.0)
      {
        //cout << "Connection timed out" << endl;
        //cout << "Connection: " << sock_d << " closed." << endl << endl;
        close(sock_d);
        return -2;
      }
      time(&end);
      
    }
    //Track total bytes read
    totalbytes += numbytes;
    data.append(buf,numbytes);
    //cout << "Data: " << data << endl;
    //If 0 bytes are read the client has closed the connection
    if(numbytes == 0)
    {
      //cout << "Connection: " << sock_d << " closed." << endl << endl;
      close(sock_d);
      return 0;
    }
    
    //If the last 4 bytes match the pattern '\r\n\r\n' then we have a complete request
    if(data.find("\r\n\r\n") != string::npos)
    {
        break;
    }
   
  }
  if(isresponse)
  {
    const char* tempBuf = NULL;
    tempBuf = data.c_str();
    headersize = GetHeaderStats(tempBuf,data.find("\r\n\r\n")+4,&hbuf);
    head.ParseHeaders(hbuf,headersize);
    datasize = atoi(head.FindHeader("Content-Length").c_str());
    while(1)
    {
      //Will be used to track connection that has timed out
      time(&start);
      time(&end);
      
      //Polling read
      while((numbytes = recv(sock_d, buf, BUFSIZE, 0)) == -1) 
      {
        //cout << "Error: recv" << endl;
        //return 1;
        //cout << numbytes << endl;
        if(difftime (end,start) > 15.0)
        {
          //cout << "Connection timed out" << endl;
          //cout << "Connection: " << sock_d << " closed." << endl << endl;
          close(sock_d);
          return -2;
        }
        time(&end);
      }
      //Track total bytes read
      dataread += numbytes;
      totalbytes += numbytes;
      data.append(buf,numbytes);
      //If 0 bytes are read the client has closed the connection
      if(numbytes == 0)
      {
        //cout << "Connection: " << sock_d << " closed." << endl << endl;
        close(sock_d);
        return totalbytes;
      }
      //cout << "Read: " << dataread << endl;
      //cout << "Expect: " << datasize << endl;
      if(dataread == datasize)
      {
          break;
      }
     
    }
  }
  return totalbytes;
  
}


void * HandleClient(void * sock)
{
  int sock_client = *((int*) sock);
  //bool closeConnection = false;
  int sock_server;
  int numbytes;
  //int datasize;
  //int headersize;
  int totalbytes = 0;
  //char cbuf[BUFSIZE];
  //char sbuf[BUFSIZE];
  //char* dbuf = NULL; 
  //char* hbuf = NULL;
  stringstream ss,test;
  size_t tempBufSize;
  char* tempBuf;
  struct addrinfo hostai, *result;
  string cdata = "";
  string sdata = "";
  
  
  while(1)
  {
    cdata = "";
    sdata = "";
    totalbytes = ReceiveHttpData(sock_client,cdata,false);
    //cout << "Recieve status: " << totalbytes << endl;
    if(totalbytes == 0)
    {
      pthread_mutex_lock(&tcount_mutex);
      threadCount--;
      pthread_mutex_unlock(&tcount_mutex);
      pthread_exit(NULL);
    }
  
    HttpRequest req;
    HttpResponse rep;
    HttpHeaders head;
    
    try
    {
      req.ParseRequest(cdata.c_str(),totalbytes);
    }
    catch (ParseException ex)
    {
      string cmp = "Request is not GET";
      if (ex.what()==cmp)
      {
        rep.SetStatusCode("501");
        rep.SetStatusMsg("Not Implemented");
        rep.SetVersion(req.GetVersion());      	
      }
      else 
      {
        rep.SetStatusCode("400");
        rep.SetStatusMsg("Bad Request");
        rep.SetVersion(req.GetVersion());
      }
      
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
      rep.FormatResponse(tempBuf);
      //cout << "Sent " << 
      send(sock_client, tempBuf, tempBufSize,0);
      // << " bytes" << endl;
      free(tempBuf);
      pthread_mutex_lock(&tcount_mutex);
      threadCount--;
      pthread_mutex_unlock(&tcount_mutex);
      pthread_exit(NULL);
    } 
    
    string version = req.GetVersion();
    if(version == "1.0")
      req.ModifyHeader("Connection", "close");
      
    //if(head.FindHeader("Connection") == "close")
    //  closeConnection = true;
    //head.ParseHeaders(cdata.c_str(),totalbytes);
    
    //TODO
    //Need to check all the headers(expiration, etc) not just Connection: close
    //Same goes for information recieved from server further down.
    
    //ss << "Method: " << req.GetMethod() << endl;
    //ss << "Host: " << req.GetHost() << endl;
    //ss << "Port: " << req.GetPort() << endl;
    //ss << "Path: " << req.GetPath() << endl;
    //ss << "Version: " << req.GetVersion() << endl;
    
    
    //cout << ss.str();
    
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
    ss.str("");
    ss << req.GetHost() << req.GetPort() << req.GetPath();
    string HostPath = ss.str();		
    if (!isCached(HostPath)) 
    {
    	sock_server = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
    	connect(sock_server,result->ai_addr, result->ai_addrlen);
    
    	//Allocate space to build the HTTP request
    	tempBufSize = req.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
    
    	req.FormatRequest(tempBuf);    
      ss.str("");

      //cout << "Sent " << 
      send(sock_server,tempBuf,tempBufSize,0); 
      //<< " bytes to server" << endl;
  
      free(tempBuf);
      //cout << "??" << endl;
      numbytes = ReceiveHttpData(sock_server,sdata,true);
      //cout << "Recv " << numbytes << " from server." << endl;

      pthread_mutex_lock(&cache_mutex);
      PageCache[HostPath] = sdata;
      pthread_mutex_unlock(&cache_mutex);

      rep.ParseResponse(sdata.c_str(),numbytes);

      //ss.str("");
  
      //ss << "Version: " << rep.GetVersion() << endl;
      //ss << "Status Code: " << rep.GetStatusCode() << endl;
      //ss << "Status Message: " << rep.GetStatusMsg() << endl;
      //cout << ss.str();
  

      close(sock_server);
      
    }
    else //Cached, get it from internal memory and send it.
    {  
      //cout << "Cached!!!" << endl;
      pthread_mutex_lock(&cache_mutex);
      sdata = PageCache[HostPath];
      pthread_mutex_unlock(&cache_mutex);
      numbytes = sdata.size();
      
    }



    //cout << "Sent " << 
    send(sock_client, sdata.c_str(), numbytes,0); 
    //<< " bytes header" << endl;
    //cout << "Sent " << send(sock_client, sbuf+tempBufSize, datasize,0) << " bytes data" << endl;


    if(head.FindHeader("Connection") == "close")
    {
      //cout << "HEADER Connection: close found" << endl;
      //cout << "Connection: " << sock_client << " closed." << endl << endl;
      close(sock_server);
      close(sock_client);
      pthread_mutex_lock(&tcount_mutex);
      threadCount--;
      pthread_mutex_unlock(&tcount_mutex);
      pthread_exit(NULL);
      
    }
  }
  pthread_mutex_lock(&tcount_mutex);
  threadCount--;
  pthread_mutex_unlock(&tcount_mutex);
  pthread_exit(NULL);
}



int main (int argc, char *argv[])
{
  pthread_mutex_init(&cache_mutex, NULL);
  pthread_mutex_init(&tcount_mutex, NULL);
  //http-proxy called without command-line parameters
  if (argc != 1)
    {
      //cout << "Cannot include parameters with http-proxy call\n";
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
		//cout << "Error: getaddrinfo() returned " << status << endl;
		return 1;
	} 
  
  sock_d = socket(result->ai_family,result->ai_socktype,result->ai_protocol);  //create socket with hostai properties, 0 for the protocol field automatically chooses the correct protocol
  if(sock_d == -1)
  {
		//cout << "Error: socket() failed returned " << sock_d;
		return 2;
  }
  
  status = bind(sock_d, result->ai_addr, result-> ai_addrlen); //bind socket to result addrinfo
	if(status) 
	{
		//cout << "Error: bind() returned " << status << endl;
    //cout << "Errno: " << strerror (errno) << endl;
		return 3;
	} 

  int listenresult = listen(sock_d,15);  //silently limits incoming queue to 15
  if(listenresult)
  {
    return 4;
  }
  //print the sd because warnings treated as errors
  //cout << "socketfd: " << sock_d << endl;
  //cout << "listenresult: " << listenresult << endl;
  //Broke these lines up and replaced '\n' with endl and it stopped 
  //getting stuck and accepted my telnet requests.
  
  while(1)
  {
    addr_size = sizeof their_addr;

    sock_new = accept(sock_d, (struct sockaddr *)&their_addr, &addr_size);  //accept the connection

    //cout << "Number of procs " << threadCount << endl;
    if (threadCount > MAXNUMTHREAD)
    {
      int tempBufSize;
      char* tempBuf;
      //Error Server busy/full
      //cout << "==========================" << endl;
      //cout << "-------Server Full-------" << endl;
      //cout << "==========================" << endl;
      HttpResponse rep;
      HttpHeaders head;
      rep.SetStatusCode("503");
      rep.SetStatusMsg("Service Unavailable");
      rep.SetVersion("1.1");
      head.AddHeader("Connection","close");
      tempBufSize = rep.GetTotalLength();
      tempBuf = (char*)malloc(tempBufSize);
      rep.FormatResponse(tempBuf);
      //cout << "Sent " << 
      send(sock_new, tempBuf, tempBufSize,0); 
      //<< " bytes" << endl;
      close(sock_new);
      free(tempBuf);
    }
    else if(sock_new)
    {
      //if (!fork()) // this is the child process 
      //{
        //close(sock_d);  // child doesn't need the listener
        //HandleClient(sock_new);
        //return 0;
      //}
      //cout << "Connection: " << sock_new << " opened." << endl;
      //close(sock_new); //Getting "Address alread in use" error immediatly after a run... thought this would release the socket and fix it.
      pthread_mutex_lock(&tcount_mutex);
      threadCount++;
     
 
      pthread_t client;
      pthread_create(&client,NULL,HandleClient,(void*)&sock_new);
      pthread_mutex_unlock(&tcount_mutex);
    }  
  }
  
  return 0;
}
