/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

int main (int argc, char *argv[])
{
  //http-proxy called without command-line parameters
  if (argc != 0)
    {
      cout << "Cannot include parameters with http-proxy call";
      return 1;
    }

  // command line parsing
  
  return 0;
}
