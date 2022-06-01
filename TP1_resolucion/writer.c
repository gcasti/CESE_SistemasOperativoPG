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
static char const * const format_sig = "SIGN: %d";

volatile sig_atomic_t fd;
char msg_buffer[BUFFER_SIZE];

void receiveSIGUSER1(int sig){
    snprintf(msg_buffer,BUFFER_SIZE,format_sig,1);
}
void receiveSIGUSER2(int sig){
    write(fd, "Receive SIGUSER2\n", 20);
}

int main(void)
{
   
    char outputBuffer[BUFFER_SIZE];
    //char msg_buffer[BUFFER_SIZE];
	uint32_t bytesWrote;
	int32_t returnCode;
    int32_t count_characters = 0;
    pid_t pid_process;
    struct sigaction siguser1;
    struct sigaction siguser2;
    
    siguser1.sa_handler = receiveSIGUSER1;
    siguser2.sa_handler = receiveSIGUSER2;

    if (sigaction(SIGUSR1,&siguser1,NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    if (sigaction(SIGUSR2,&siguser2,NULL) == -1) {
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
		fgets(outputBuffer, BUFFER_SIZE, stdin);
        
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
	return 0;
}
