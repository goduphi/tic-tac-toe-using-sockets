#include <stdio.h>
#include <pthread.h>
#include "socket.h"

static const int MAXPENDING = 2; // Maximum number of outstanding connection requests

void CheckCmdArgs(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Follow the format: %s <Server Address> <Port Number>", argv[0]);
		exit(0);
	}
}

void *SendDataToAllClients(void *clientData)
{
	Clients *clients = (Clients *)clientData;
	int id = clients->id;
	printf("Client id: %d, joined the server.\n", id);
	while(1)
	{
		memset(clients->data, 0, sizeof(BUFFSIZE));
		ssize_t bytesReceived = ServerReceiveData(clients->descriptors[id], clients->data);
		
		if(bytesReceived > 0)
		{
			printf("Received %ld bytes of data from client: %d -> %s\n", bytesReceived, id, clients->data);
			// Debugging
			int i = 0;
			for(i = 0; i < BUFFSIZE; i++)
			{
				printf("%x ", clients->data[i]);
			}
			printf("\n");
			for(i = 0; i < MAXPENDING; i++)
				// Send the data to all clients except the sender
				if(clients->descriptors[i] != clients->descriptors[id])
					Send(clients->descriptors[i], clients->data);
		}
	}
	close(clients->descriptors[id]);
}

int main(int argc, char *argv[])
{
	// Check for proper number of arguments
	CheckCmdArgs(argc, argv);
	
	in_port_t Port = atoi(argv[1]);	// Local port
	
	// Create a synchronous socket
	int SocketDescriptor = CreateSyncSocket();
	
	SocketAddress ServAddr;
	ConstructServerPort(&ServAddr, Port);
	
	// Bind to the local address
	if(bind(SocketDescriptor, (struct sockaddr *)&ServAddr, sizeof(ServAddr)) < 0)
		perror("bind() failed");
	
	// Mark the socket so it will listen for incoming connections
	if(listen(SocketDescriptor, MAXPENDING) < 0)
		perror("listen() failed");
	
	printf("Waiting for connections ...\n");
	
	Clients clients;
	memset(&clients, 0, sizeof(clients));
	
	// Keeps track of the number of clients
	int ClientCount = 0;
	pthread_t ClientThread[MAXPENDING];
	
	char *DataBuffer = (char *)malloc(sizeof(BUFFSIZE));
	clients.data = DataBuffer;
	
	// You want to accept all incoming connections
	while(1)
	{
		SocketAddress ClientAddr;	// Client Address
		// Set the length of the client address structure
		socklen_t ClientAddrLen = sizeof(ClientAddr);
		
		// Wait for client connect
		int sock = accept(SocketDescriptor, (struct sockaddr *)&ClientAddr, &ClientAddrLen);
		
		if(sock < 0)
		{
			perror("accept() failed");
			exit(0);
		}
		
		clients.id = ClientCount;
		clients.descriptors[ClientCount] = sock;
		
		char ClientName[INET_ADDRSTRLEN];
		if(inet_ntop(AF_INET, &ClientAddr.sin_addr.s_addr, ClientName, sizeof(ClientName)) != NULL)
			printf("Handling client %s/%d\n", ClientName, ntohs(ClientAddr.sin_port));
		else
			printf("Unable to get client address.\n");
		
		// Instantiate thread to receive and send data to client
		if(pthread_create(&ClientThread[ClientCount++], NULL, SendDataToAllClients, (void *)&clients))
		{
			printf("Failed to instantiate thread for client %d.\n", ClientCount);
		}
		
		if(ClientCount == 2)
			ClientCount = 0;
	}
	
	free(DataBuffer);
	int i = 0;
	for(i = 0; i < MAXPENDING; i++)
		pthread_join(ClientThread[i], NULL);
	
	return EXIT_SUCCESS;
}