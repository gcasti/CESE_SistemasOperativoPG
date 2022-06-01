#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300
#define HEADER_DATA "DATA"
#define HEADER_SIGNAL "SIGN"
#define SIZE_OF_HEADER 4
const char *file_log_name = "log.txt";
const char *file_sig_name = "signal.txt";


int main(void)
{
	uint8_t inputBuffer[BUFFER_SIZE];
	int32_t bytesRead, returnCode, fd;
    FILE *fp_log_data , *fp_log_signal;
    char *p = NULL;
    int i = 0;
    char header[SIZE_OF_HEADER];

    
    /* Create named fifo. -1 means already exists so no action if already exists */
    if ( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) ) < -1  )
    {
        printf("Error creating named fifo: %d\n", returnCode);
        exit(1);
    }
    
    /* Open named fifo. Blocks until other process opens it */
	printf("waiting for writers...\n");
	if ( (fd = open(FIFO_NAME, O_RDONLY) ) < 0 )
    {
        printf("Error opening named fifo file: %d\n", fd);
        exit(1);
    }
    
    /* open syscalls returned without error -> other process attached to named fifo */
	printf("got a writer\n");

    /* open file for logging data */
    fp_log_data = fopen(file_log_name,"w+");
    if (fp_log_data == NULL) 
    {
        perror ("File error");
    }
    fprintf(fp_log_data, "%s \n", "File log data data receive \n"); 

    /* open file for logging signal */ 
    fp_log_signal = fopen(file_sig_name,"w+");
    if (fp_log_signal == NULL) 
    {
        perror ("File error");
    }
    fprintf(fp_log_signal, "%s \n", "File log signal receive \n"); 
    
    /* Loop until read syscall returns a value <= 0 */
	do
	{
        /* read data into local buffer */
		if ((bytesRead = read(fd, inputBuffer, BUFFER_SIZE)) == -1)
        {
			perror("read");
        }
        else
		{   /* Processing data receive*/
			inputBuffer[bytesRead] = '\0';
            
			printf("reader: read %d bytes: \"%s\"\n", bytesRead, inputBuffer);
            
            for(i=0 ; i < SIZE_OF_HEADER ; i++)
            {
                header[i] = inputBuffer[i];
            }
                    
            /* Log data or signal */
            
            if(0 == strncmp(header,HEADER_DATA,SIZE_OF_HEADER))
            {
                int count = fprintf(fp_log_data, "%s \n", inputBuffer); 
                printf("Logging %d \n",count);
            }else{
                if(0 == strncmp(header,HEADER_SIGNAL,SIZE_OF_HEADER))
                {
                    int count = fprintf(fp_log_signal, "%s \n", inputBuffer);
                    printf("Logging %d \n",count);
                }else{
                    printf("Error data received \n");
                }
            }
		}
	}
	while (bytesRead > 0);
    
    fclose(fp_log_data);
    fclose(fp_log_signal);

	return 0;
}
