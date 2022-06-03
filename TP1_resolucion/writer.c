/*
 Autor: Guillermo Luis Castiglioni
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>


#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300

static char const * const format_msg = "DATA: %s";
static char const * const msg_sig1 = "SIGN: 1";
static char const * const msg_sig2 = "SIGN: 2";

volatile sig_atomic_t fd;

void receiveSIGUSER1(int sig){
   int32_t bytesWroteSig = 0;
   if( bytesWroteSig == write(fd, msg_sig1, sizeof(msg_sig1)) == -1 )
   {
       perror("Error Signal");
   }else{
       printf("Signal 1 received \n");
   };
}
void receiveSIGUSER2(int sig){
    int32_t bytesWroteSig = 0;
   if( bytesWroteSig == write(fd, msg_sig2, sizeof(msg_sig2)) == -1 )
   {
       perror("Error Signal");
   }else{
       printf("Signal 2 received \n");
   };
}

int main(void)
{
    char outputBuffer[BUFFER_SIZE];
    char msg_buffer[BUFFER_SIZE];
	uint32_t bytesWrote;
	int32_t returnCode , count_characters = 0;
    pid_t pid_process;
    struct sigaction sa1 , sa2;
       
    sa1.sa_handler = receiveSIGUSER1;
    sa1.sa_flags = 0; //SA_RESTART;
	sigemptyset(&sa1.sa_mask);
    sa2.sa_handler = receiveSIGUSER2;
    sa2.sa_flags = 0; //SA_RESTART;
	sigemptyset(&sa2.sa_mask);

    if (sigaction(SIGUSR1,&sa1,NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    if (sigaction(SIGUSR2,&sa2,NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    pid_process = getpid();

    printf("Process writer, PID %d\n",getpid());

    /* Create named fifo. -1 means already exists so no action if already exists */
    if ( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) ) < -1 )
    {
        printf("Error creating named fifo: %d\n", returnCode);
        exit(1);
    }

    /* Open named fifo. Blocks until other process opens it */
	printf("waiting for readers...\n");
	if ( (fd = open(FIFO_NAME, O_WRONLY) ) < 0 )
    {
        printf("Error opening named fifo file: %d\n", fd);
        exit(1);
    }
    
    /* open syscalls returned without error -> other process attached to named fifo */
	printf("got a reader--type some stuff\n");

    /* Loop forever */
	while (1)
	{
        /* Get some text from console */
		if (fgets(outputBuffer, BUFFER_SIZE, stdin) == NULL)
        {
            perror("fgets");
        }else{
            count_characters = snprintf(msg_buffer,BUFFER_SIZE,format_msg,outputBuffer);

            /* Write buffer to named fifo. Strlen - 1 to avoid sending \n char */
            if ((bytesWrote = write(fd, msg_buffer, strlen(msg_buffer)-1)) == -1)
            {
                perror("write");
            }
            else
            {
                printf("writer: wrote %d bytes\n", bytesWrote);
            }
        }
	}
	return 0;
}
