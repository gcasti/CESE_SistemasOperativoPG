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
#include <signal.h>
#include "SerialManager.h"

pthread_mutex_t mutex_serial = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_socket = PTHREAD_MUTEX_INITIALIZER;
bool serial_connected = false;
bool socket_connected = false;
bool end_program = false;
int fd_client_socket;
int fd_server_socket;
int fd;

void *connection_serial_emulator(void *arg);
void *connection_interface_service(void *arg);
static int signal_action(int action, int signal);
static int config_handler_signal(int signal, void *functionSignal);
static void signal_cb(void);

/* Se utiliza un hilo auxiliar para gestionar la conexión con InterfaceService empleando un socket
 *  En el hilo principal se gestiona la conexión con el Emulador del puerto serie, manejo de señales
 *  y finalización de todos los recursos empleados */

int main(void)
{
	printf("*************************\r\n");
	printf("* Inicio Serial Service *\r\n");
	printf("* PID %d             *\r\n", getpid());
	printf("*************************\r\n");

	// Configuración de handlers de señales
	if (config_handler_signal(SIGINT, signal_cb) != 0)
	{
		perror("Error config handler SIGINT");
	}
	if (config_handler_signal(SIGTERM, signal_cb) != 0)
	{
		perror("Configuración handler SIGINT");
	}

	// Bloqueo de señales para los hilos que se crearán
	if (signal_action(SIG_BLOCK, SIGINT) < 0)
	{
		perror("Error bloqueo señal SIGINT");
	}
	if (signal_action(SIG_BLOCK, SIGTERM) < 0)
	{
		perror("Error bloqueo señal SIGTERM");
	}

	// Creación de un thread para establecer la comunicación con la InterfaceService
	pthread_t thread_interface_service;
	int retVal;
	retVal = pthread_create(&thread_interface_service, NULL, connection_interface_service, NULL);
	if (retVal < 0)
	{
		perror("Error al crear thread: thread_interface_service");
		exit(1);
	}

	// Se desbloquean las señales de forma que el hilo principal pueda manejarlas
	if (signal_action(SIG_UNBLOCK, SIGINT) < 0)
	{
		perror("Error desbloqueo señal SIGINT");
	}
	if (signal_action(SIG_UNBLOCK, SIGTERM) < 0)
	{
		perror("Error desbloqueo señal SIGTERM");
	}

	// Conexión con el Emulador del puerto serie
	/* Nota: La función suministrada permanece esperando que se establezca
	 la conexión debería modificarse para ejecutar un cierre correcto del programa */
	int count_read = 0;
	char buffer_serial[10];
	bool check_state = false;

	if (serial_open(1, 115200) != 0)
	{
		printf("Error abriendo puerto serie \r\n");
		exit(1);
	}
	else
	{
		pthread_mutex_lock(&mutex_serial);
		serial_connected = true;
		pthread_mutex_unlock(&mutex_serial);
	}
	while (!end_program)
	{
		if (serial_receive(buffer_serial, sizeof(buffer_serial)) > 0)
		{
			printf("Serie recepción: %s\n", buffer_serial);

			// Envío del mensaje a InterfaceService
			pthread_mutex_lock(&mutex_serial);
			check_state = serial_connected;
			pthread_mutex_unlock(&mutex_serial);
			if (check_state)
			{
				if (write(fd_client_socket, buffer_serial, sizeof(buffer_serial)) < 0)
				{
					perror("Error escribiendo mensaje en socket");
					break;
				}
			}
			else
			{
				printf("InterfaceService no disponible\n");
			}
		}
		usleep(10000);
	}

	// El hilo finaliza porque las funciones accept y read bloqueantes utilizadas son puntos de cancelación
	if (pthread_cancel(thread_interface_service) != 0)
	{
		perror("Error finalización pthread");
		exit(1);
	}

	// Espera finalización del hilo
	if (pthread_join(thread_interface_service, NULL) != 0)
	{
		perror("Error función join");
		exit(1);
	}

	if (socket_connected)
	{
		if (close(fd_client_socket) < 0)
		{
			perror("Error al cerrar el socket");
		}
		printf("Cierre socket conexión con Interface Service \n");
	}
	printf("Cierre del socket server \n");
	if(close(fd_server_socket) < 0)
	{
		perror("Error al cerrar el socket");
	}

	printf("Cierre del puerto serie \n");
	serial_close();

	printf("Fin del programa \n");
	exit(EXIT_SUCCESS);
}

void *connection_interface_service(void *arg)
{
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	char buffer_socket[10];
	int receive_bytes;
	bool check_state = false;

	printf("Inicio thread connection_interface_service \n");
	fd_server_socket = socket(AF_INET, SOCK_STREAM, 0);

	// Se cargan los datos de IP:PORT del server
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(10000);

	// serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr)) <= 0)
	{
		fprintf(stderr, "ERROR invalid server IP\r\n");
		exit(1);
	}

	// Se abre el puerto esperando la conexión de algún cliente
	if (bind(fd_server_socket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		close(fd_server_socket);
		perror("listener: bind");
		exit(1);
	}
	if (listen(fd_server_socket, 10) == -1)
	{
		perror("error en listen");
		exit(1);
	}

	printf("Espera de conexión entrante \n");
	while (1)
	{

		addr_len = sizeof(struct sockaddr_in);

		if ((fd_client_socket = accept(fd_server_socket, (struct sockaddr *)&clientaddr, &addr_len)) == -1)
		{
			perror("error en accept");
			exit(1);
		}
		else
		{
			char ipClient[32];
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
			printf("server:  conexion desde:  %s\n", ipClient);

			pthread_mutex_lock(&mutex_socket);
			socket_connected = true;
			pthread_mutex_unlock(&mutex_socket);

			while (1)
			{
				// read bloqueante
				if ((receive_bytes = read(fd_client_socket, buffer_socket, sizeof(buffer_socket))) == -1)
				{
					perror("error en read");
					exit(1);
				}
				// si read devuelve 0 quiere decir que el cliente se desconectó
				if (receive_bytes == 0)
				{
					printf("*** InterfaceService desconectada ***\n");
					pthread_mutex_lock(&mutex_socket);
					socket_connected = false;
					pthread_mutex_unlock(&mutex_socket);
					break;
				}
				else
				{
					buffer_socket[receive_bytes] = 0x00;

					printf("Socket recepción: %s\n", buffer_socket);
					// Se reenvía el mensaje recibido al emulador del puerto serie
					pthread_mutex_lock(&mutex_serial);
					check_state = serial_connected;
					pthread_mutex_unlock(&mutex_serial);
					if (check_state)
					{
						serial_send(buffer_socket, receive_bytes);
					}
					else
					{
						printf("Emulador puerto serie no disponible\n");
					}
				}
			}
			close(fd_client_socket);
		}
	}
}

// Función interna que bloquea o desbloquea una señal
static int signal_action(int action, int signal)
{
	sigset_t set;
	int retVal = 0;
	sigemptyset(&set);

	if (sigaddset(&set, signal) < 0)
	{
		perror("Error sigaddset \n");
		retVal = -1;
	}
	if (pthread_sigmask(action, &set, NULL) < 0)
	{
		perror("Error sigmask \n");
		return -1;
	}
	return retVal;
}
// Función para configurar los handlers de las señales que se desean utilizar
static int config_handler_signal(int signal, void *functionSignal)
{
	struct sigaction sa;

	sa.sa_handler = functionSignal;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	if (sigaction(signal, &sa, NULL) == -1)
	{
		perror("Error sigaction \n");
		return (-1);
	}
	return 0;
}

static void signal_cb(void)
{
	write(1, "Señal recibida \n", 17);
	end_program = true;
}