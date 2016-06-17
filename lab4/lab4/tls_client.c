/*	
 *	Demonstration TLS client
 *
 *       Compile with
 *
 *       gcc -Wall -o tls_client tls_client.c -L/usr/lib -lssl -lcrypto
 *
 *       Execute with
 *
 *       ./tls_client <server_INET_address> <port> <client message string>
 *
 *       Generate certificate with 
 *
 *       openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout tls_demonstration_cert.pem -out tls_demonstration_cert.pem
 *
 *	 Developed for Intel Edison IoT Curriculum by UCLA WHI
 */
#include "tls_header.h"
#include <pthread.h>
#include <fcntl.h>

struct args_t
{
    int *rate;
    int *log_fd;
    char *sent;
    SSL *ssl;
};

void *thread_start(void *arg)
{
    struct args_t thread_args = *(struct args_t *)arg;
    int receive_length; 
    char buf[BUFSIZE];
    while(1)
    {
        memset(buf,0,sizeof(buf));
        receive_length = SSL_read(thread_args.ssl, buf, sizeof(buf));
        if(strstr(buf, "new_rate: ") != NULL)
        {
            sscanf(buf, "Heart rate of patient %*s is %*f new_rate: %d", thread_args.rate);
	    printf("New rate %d received from server.\n", *(thread_args.rate));
        }
        printf("Received message '%s' from server.\n\n", buf);
        dprintf(*(thread_args.log_fd), "Received message '%s' from server.\n", buf);
    }
}

int main(int args, char *argv[])
{
    int port, range, rate;
    int server;
    int line_length;
    char ip_addr[BUFSIZE];
	char *my_ip_addr;
    char *line = NULL;
    char buf[BUFSIZE];
    double heart_rate;
    FILE *fp = NULL;
    SSL *ssl;
    SSL_CTX *ctx;
	
	my_ip_addr = get_ip_addr();
    printf("My ip addr is: %s\n", my_ip_addr);

    /* READ INPUT FILE */
    fp = fopen("config_file", "r");
    if(fp == NULL){
	fprintf(stderr, "Error opening config file with name 'config_file'. Exiting.\n");
	exit(1);
    }
    printf("Reading input file...\n");
    while(getline(&line, &line_length, fp) > 0){
	if(strstr(line, "host_ip") != NULL){
	    sscanf(line, "host_ip: %s\n", ip_addr);
	}
	else if(strstr(line, "port") != NULL){
	    sscanf(line, "port: %d\n", &port);
	}
	else if(strstr(line, "range") != NULL){
	    sscanf(line, "range: %d\n", &range);
	}
	else if(strstr(line, "rate") != NULL){
	    sscanf(line, "rate: %d\n", &rate);
	}
	else{
	    fprintf(stderr, "Unrecognized line found: %s. Ignoring.\n", line);
	}
    }
    fclose(fp);
    /* FINISH READING INPUT FILE */

    printf("Connecting to: %s:%d\n", ip_addr, port);

    /* SET UP TLS COMMUNICATION */
    SSL_library_init();
    ctx = initialize_client_CTX();
    server = open_port(ip_addr, port);
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server);
    /* FINISH SETUP OF TLS COMMUNICATION */

    /* SEND HEART RATE TO SERVER */
    if (SSL_connect(ssl) == -1){ //make sure connection is valid
	fprintf(stderr, "Error. TLS connection failure. Aborting.\n");
	ERR_print_errors_fp(stderr);
	exit(1);
    }
    else {
	printf("Client-Server connection complete with %s encryption\n", SSL_get_cipher(ssl));
	display_server_certificate(ssl);
    }
    int log_fd = creat("Lab4-E-2.txt", S_IRWXU);
    struct args_t thread_args;
    thread_args.rate = &rate;
    thread_args.ssl = ssl;
    thread_args.log_fd = &log_fd;
    thread_args.sent = &buf;

    pthread_t thread;
    pthread_create(&thread, NULL, &thread_start, (void *)&thread_args);

    while(1){
	printf("Current settings: rate: %d, range: %d\n", rate, range);
	heart_rate = 
	    generate_random_number(AVERAGE_HEART_RATE-(double)range, AVERAGE_HEART_RATE+(double)range);
	memset(buf,0,sizeof(buf)); //clear out the buffer

	//populate the buffer with information about the ip address of the client and the heart rate
	sprintf(buf, "Heart rate of patient %s is %4.2f", my_ip_addr, heart_rate);
	printf("Sending message '%s' to server...\n", buf);
	SSL_write(ssl, buf, strlen(buf));
	sleep(rate);
    }
    /* FINISH HEART RATE TO SERVER */

    //clean up operations
    SSL_free(ssl);
    close(server);
    SSL_CTX_free(ctx);
    return 0;
}
