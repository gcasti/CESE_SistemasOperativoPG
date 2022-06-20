#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "SerialManager.h"

bool serial_connected = false;
int newfd;

void* connection_serial_emulator(void* arg);
void* connection_interface_service(void* arg);

int main(void)
{	printf("*************************\r\n");
	printf("* Inicio Serial Service *\r\n");
	printf("*************************\r\n");

	// Creación de un thread para establecer la comunicación con el emulador del puerto serie
	pthread_t thread_emulator;
	int retVal;
	retVal = pthread_create (&thread_emulator , NULL , connection_serial_emulator , NULL);
	if (retVal < 0) {
    	errno = retVal;
        perror("Error al crear thread: thread_emulator");
        return -1;
    }

	// Creación de un theread para establecer la comunicación con la InterfaceService
	pthread_t thread_interface_service;
	retVal = pthread_create (&thread_interface_service , NULL , connection_interface_service , NULL);
	if (retVal < 0) {
    	errno = retVal;
        perror("Error al crear thread: thread_interface_service");
        return -1;
    }

	pthread_join(thread_emulator , NULL);
	pthread_join(thread_interface_service , NULL);

	printf("Fin del programa \n");
}

void* connection_interface_service(void* arg) {
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	char buffer_socket[10];
	int receive_bytes;
    int fd_server_socket;

	printf("Inicio thread connection_interface_service \n");
	fd_server_socket = socket(AF_INET,SOCK_STREAM, 0);

	// Se cargan los datos de IP:PORT del server
    bzero((char*) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);
    
	//serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr))<=0)
    {
       	fprintf(stderr,"ERROR invalid server IP\r\n");
       	exit(1);
    }

	// Se abre el puerto esperando la conexión de algún cliente
	if (bind(fd_server_socket , (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
		close(fd_server_socket);
		perror("listener: bind");
		exit(1);
	}
	if (listen (fd_server_socket, 10) == -1) // backlog=10
  	{
    	perror("error en listen");
    	exit(1);
  	}
 
	printf("Espera de conexión entrante \n");
	while(1){
		
		addr_len = sizeof(struct sockaddr_in);
		
    	if ( (newfd = accept(fd_server_socket, (struct sockaddr *)&clientaddr,&addr_len)) == -1)
      	{
		    perror("error en accept");
		    exit(1);
	    }else{
			char ipClient[32];
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
			printf  ("server:  conexion desde:  %s\n",ipClient);

			while(1)
			{
				// read bloqueante	
            	if( (receive_bytes = read(newfd, buffer_socket, sizeof(buffer_socket))) == -1 )
				{
                	perror("error en read");
                	exit(1);
           		}
				// si read devuelve 0 quiere decir que el cliente se desconecto
				if(receive_bytes == 0)
				{
					printf("*** InterfaceService desconectada ***\n");
					break;
				}else{
					buffer_socket[receive_bytes]=0x00;

            		printf("Socket: recepción %d bytes: %s\n", receive_bytes , buffer_socket);
					// Se reenvía el mensaje recibido al emulador del puerto serie
					if(serial_connected){
						serial_send(buffer_socket,receive_bytes);
					}else{
						printf("Emulador puerto serie no disponible\n");
					}
				}		
				sleep(1);
			}	
			close(newfd);
		}
	}
}


// Conexión con el emulador del puerto serie
void* connection_serial_emulator(void* arg){
	int count_read = 0;
	char buffer_serial[10];
	
	printf("Inicio thread connection_serial_emulator \n");

	if(serial_open(1,115200) != 0)
	{
		printf("Error abriendo puerto serie \r\n");
		serial_connected = false;
	}else{
		printf("Emulador puerto serie conectado \n");
		serial_connected = true;
	}
	while(1)
	{
		
		if (serial_receive(buffer_serial , sizeof(buffer_serial)) > 0)
		{
			printf("Serie: recepción : %s\n", buffer_serial);
			//Envío a la InterfaceService
			if (write (newfd, buffer_serial, sizeof(buffer_serial)) == -1)
    		{
      			perror("Error escribiendo mensaje en socket");
      			exit (1);
    		}
		}
		sleep(1);
	}
	serial_close();
	serial_connected = false;
	exit(EXIT_SUCCESS);
}