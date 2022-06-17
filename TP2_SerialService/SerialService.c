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

static char const * const format_in  = ">SW:%d,%d\r\n";
static char const * const format_out = ">OUT:%d,%d\r\n";

bool serial_connected = false;
int newfd;

void* connection_serial_emulator(void* arg);

int main(void)
{	printf("*************************\r\n");
	printf("* Inicio Serial Service *\r\n");
	printf("*************************\r\n");

	int bytes_read;

	pthread_t thread_emulator;
	int retVal;
	retVal = pthread_create (&thread_emulator,NULL, connection_serial_emulator ,NULL);
	if (retVal < 0) {
    	errno = retVal;
        perror("pthread_create serial");
        return -1;
    }

	// Socket	
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	char buffer[128];
	int n;
    int fd_server_socket;

	// Creamos socket
	fd_server_socket = socket(AF_INET,SOCK_STREAM, 0);

	// Cargamos datos de IP:PORT del server
    bzero((char*) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);
    
	//serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr))<=0)
    {
       	fprintf(stderr,"ERROR invalid server IP\r\n");
       	return 1;
    }

	// Abrimos puerto con bind()
	if (bind(fd_server_socket , (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
		close(fd_server_socket);
		perror("listener: bind");
		return 1;
	}

	// Seteamos socket en modo Listening
	if (listen (fd_server_socket, 10) == -1) // backlog=10
  	{
    	perror("error en listen");
    	exit(1);
  	}

	// Conexión socket 
	printf("Espera de conexión entrante \n");
	while(1){
		// Ejecutamos accept() para recibir conexiones entrantes
		addr_len = sizeof(struct sockaddr_in);
		// solo acepta un cliente
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
            	if( (n = read(newfd, buffer, 128)) == -1 )
				{
                	perror("ERROR: READ");
                	exit(1);
           		}
				// si read devuelve 0 quiere decir que el cliente se desconecto
				if(n == 0)
				{
					printf("Cliente desconectado\n");
					break;
				}else{
					buffer[n]=0x00;
            		printf("Recibi %d bytes.:%s\n", n , buffer);
					if(serial_connected){
						serial_send(buffer,n);
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
	char buf[10];
	
	if(serial_open(1,115200)!=0)
	{
		printf("Error abriendo puerto serie \r\n");
		serial_connected = false;
	}else{
		printf("Emulador puero serie conectado \n");
		serial_connected = true;
	}
	while(1)
	{
		
		if (serial_receive(buf, sizeof(buf)) > 0)
		{
			printf("Datos recibidos desde el emulador %s",buf);			
			if (write (newfd, buf, sizeof(buf)) == -1)
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
	return 0;
}