#include <boost/foreach.hpp>
#include <boost/cstdint.hpp>
#ifndef foreach
#define foreach         BOOST_FOREACH
#endif

#include <iostream>
#include "time.h"
#include <list>
using namespace std;

#include "target.h"
#ifdef _WINDOWS
#ifndef WIN32_LEAN_AND_MEAN   
#define WIN32_LEAN_AND_MEAN   
#endif   
//#include <sal.h>
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include "process.h"
#ifndef socket_t 
#define socklen_t int
#endif   
#pragma comment(lib, "iphlpapi.lib")  
#pragma comment(lib, "ws2_32.lib")
// #pragma comment(lib, "ws2.lib")   // for windows mobile 6.5 and earlier 
#endif
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>   
#include <time.h>   
#include <sys/types.h>
#include <sys/timeb.h>
#include <math.h>


#include "helperClasses.h"
#include "someFunctions.h"
#define USE_ROUTING 0
#define TARGET_NUMBER_OF_NEIGHBORS 3

void removeOldNeighbors(list<NeighborInfo> &bidirectionalNeighbors,
	list<NeighborInfo> &unidirectionalNeighbors,
	bool &searchingForNeighborFlag,
	NeighborInfo &tempNeighbor);
void sendHelloToAllNeighbors(bool searchingForNeighborFlag,
	list<NeighborInfo> &bidirectionalNeighbors,
	list<NeighborInfo> &unidirectionalNeighbors,
	NeighborInfo &tempNeighbor,
	HostId &thisHost,
	UdpSocket &udpSocket,
	RoutingTable &routingTable);
void receiveHello(Packet &pkt,
	bool &searchingForNeighborFlag,
	list<NeighborInfo> &bidirectionalNeighbors,
	list<NeighborInfo> &unidirectionalNeighbors,
	NeighborInfo &tempNeighbor,
	HostId &thisHost);

void makeRoutingTable(RoutingTable &routingTable, list<NeighborInfo> &bidirectionalNeighbors, HostId &thisHost);
void dumpNeighbors(list<NeighborInfo> &bidirectionalNeighbors, HostId &thisHost);
int main(int argc, char* argv[])
{

#ifdef _WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0)
	{
		printf("WSAStartup failed: %d\n", GetLastError());
		return(1);
	}
#endif
	RoutingTable routingTable; // new for routing

	bool searchingForNeighborFlag = false;
	list<NeighborInfo> bidirectionalNeighbors;
	list<NeighborInfo> unidirectionalNeighbors;
	list<HostId> allHosts;
	NeighborInfo tempNeighbor;
	HostId thisHost;

	if (argc != 3)
	{
		printf("usage: MaintainNeighbors PortNumberOfThisHost FullPathAndFileNameOfListOfAllHosts\n");
		return 0;
	}

#ifdef _WINDOWS
	srand(_getpid());
#else
	srand(getpid());
#endif

	fillThisHostIP(thisHost);
	thisHost._port = atoi(argv[1]);
	readAllHostsList(argv[2], allHosts, thisHost);

	printf("Running on ip %s and port %d\n", thisHost._address, thisHost._port);
	printf("press any  key to begin\n");
	//char c = getchar();

	time_t lastTimeHellosWereSent;
	time(&lastTimeHellosWereSent);
	time_t currentTime, lastTimeStatusWasShown;
	time(&lastTimeStatusWasShown);



	UdpSocket udpSocket;
	if (udpSocket.bindToPort(thisHost._port) != 0) {
		cout << "bind failed\n";
		exit(-1);
	}

	bool searchForNeighborFlag = FALSE;

	readAllHostsList(argv[2], allHosts, thisHost);

    HelloMessage helloMessage;
	Packet pkt;
	while (1)                            // get message and set neighbor list.
	{

		pkt.getPacketReadyForWriting();
		
		if (udpSocket.checkForNewPacket(pkt, 2) > 0)  // get HostId from packet
		{

			helloMessage.getFromPacket(pkt);

			bool unilist = false;
			bool bidlist = false;
			foreach(NeighborInfo & neighborInfo, unidirectionalNeighbors)
			{
				if (neighborInfo.hostId == helloMessage.source)
				{
					
					neighborInfo.updateTimeToCurrentTime();
					foreach(HostId &hostId, helloMessage.neighbors)
					{
						if (thisHost == hostId)
						{


							//remove from unidrectionalneighbors and add in biedirectionalneighbors

							bidirectionalNeighbors.push_back(neighborInfo);  //add in
							unidirectionalNeighbors.remove(neighborInfo);
							break;
						}


					}
					unilist = true;
					break;
				}
			}

			
			foreach(NeighborInfo & neighborInfo, bidirectionalNeighbors)
			{
				if (neighborInfo.hostId == helloMessage.source)
				{	bool ifneighbor = false;
					neighborInfo.updateTimeToCurrentTime();
					bidlist = true;
					
					foreach(HostId &hostId, helloMessage.neighbors)
					{
						if (thisHost == hostId)
						{


							ifneighbor = true;
							break;
						}

					}
					if (ifneighbor == false)
					{





						neighborInfo.updateTimeToCurrentTime();
						unidirectionalNeighbors.push_back(neighborInfo);
						bidirectionalNeighbors.remove(neighborInfo);



					}
					break;
				}
			}







			if (unilist == false && bidlist == false)
			{	bool ifNeighbor = false;
				searchingForNeighborFlag == false;
				
				foreach(HostId &hostId, helloMessage.neighbors)
				{
					if (hostId == thisHost)
					{
						
						NeighborInfo neighborInfo;
						neighborInfo.hostId = helloMessage.source;
						neighborInfo.updateTimeToCurrentTime();
						bidirectionalNeighbors.push_back(neighborInfo);
						ifNeighbor = true;
						break;
					}


				}

				if (ifNeighbor == false)
				{
					NeighborInfo neighborInfo;
					neighborInfo.hostId = helloMessage.source;
					neighborInfo.updateTimeToCurrentTime();
					unidirectionalNeighbors.push_back(neighborInfo);

				}
			}




		}








		if ((bidirectionalNeighbors.size() + unidirectionalNeighbors.size()) <= TARGET_NUMBER_OF_NEIGHBORS && searchingForNeighborFlag == false)   //the ways we set the flag and search temp
		{
			searchingForNeighborFlag = true;
			tempNeighbor = selectNeighbor(bidirectionalNeighbors, unidirectionalNeighbors, allHosts, thisHost);
			tempNeighbor.updateTimeToCurrentTime();

		}
		time(&currentTime);
		if ((difftime(currentTime, tempNeighbor.timeWhenLastHelloArrived) >= 40) && searchingForNeighborFlag == true)
		{
			searchingForNeighborFlag = false;
		}

		foreach(NeighborInfo &neighborInfo, unidirectionalNeighbors)     //the condition that we need to remove unidirectional nei
		{
			time(&currentTime);
			if (difftime(currentTime, neighborInfo.timeWhenLastHelloArrived) >= 40)
			{

				unidirectionalNeighbors.remove(neighborInfo);
				break;
			}
		}


		foreach(NeighborInfo &neighborInfo, bidirectionalNeighbors)    // the condition that we need to  remove bid
		{
			time(&currentTime);
			if (difftime(currentTime, neighborInfo.timeWhenLastHelloArrived) >= 40)
			{

				removeFromList(neighborInfo, bidirectionalNeighbors);
				break;
			}
		}

		

		time(&currentTime);                 //set hello message and put into packet
		if (difftime(currentTime, lastTimeHellosWereSent) >= 10)
		{
			pkt.getPacketReadyForWriting();

			
			helloMessage.source = thisHost;
			helloMessage.type = HELLO_MESSAGE_TYPE;
			time(&currentTime);
			lastTimeHellosWereSent = currentTime;
			foreach(NeighborInfo &neighborInfo, bidirectionalNeighbors)
			{
				helloMessage.addToNeighborsList(neighborInfo.hostId);


			}
			foreach(NeighborInfo &neighborInfo, unidirectionalNeighbors)
			{
				helloMessage.addToNeighborsList(neighborInfo.hostId);

			}





			helloMessage.addToPacket(pkt);
			foreach(NeighborInfo &neighborInfo, unidirectionalNeighbors)
			{
				udpSocket.sendTo(pkt, neighborInfo.hostId);


			}
			foreach(NeighborInfo &neighborInfo, bidirectionalNeighbors)
			{
				udpSocket.sendTo(pkt, neighborInfo.hostId);


			}
			if (searchingForNeighborFlag) {
				udpSocket.sendTo(pkt, tempNeighbor.hostId);
			}
		}


		time(&currentTime);
		if (difftime(currentTime, lastTimeStatusWasShown) >= 10) {
			time(&lastTimeStatusWasShown);
			showStatus(searchingForNeighborFlag,
				bidirectionalNeighbors,
				unidirectionalNeighbors,
				tempNeighbor,
				thisHost,
				TARGET_NUMBER_OF_NEIGHBORS);
		}


	}
}
