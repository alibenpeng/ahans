#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>  /* String function definitions */
#include <signal.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <pthread.h>
#include <sys/types.h> 
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <curl/curl.h>
#include <microhttpd.h>

#define BAUDRATE B57600
#define SERIAL_PORT "/dev/ttyS1"

#define POWER_LED_FILE "/proc/diag/led/power"

#define HTTPD_PORT 8888
#define TCP_PORT 4223

#define FALSE 0
#define TRUE 1

#define NUM_COUNTERS 5
#define NUM_INSTALLED_COUNTERS 3
#define COUNTER1 0
#define COUNTER2 1
#define COUNTER3 2
#define COUNTER4 3
#define COUNTER5 4
#define COUNTER1_URL "http://lardass/middleware.php/data/46b27ba0-6ae1-11e1-91d0-4315961506a9.json?operation=add&value=%d"
#define COUNTER2_URL "http://lardass/middleware.php/data/60435230-6ae1-11e1-bc8b-7f8bbcdebe7a.json?operation=add&value=%d"
#define COUNTER3_URL "http://lardass/middleware.php/data/65c072e0-6ae1-11e1-879a-7bc14050b5a0.json?operation=add&value=%d"

volatile int STOP=FALSE; 

char empty_page[65535];
char *page = empty_page;

int serial_fd; /* File descriptor for the port */
char serial_buffer[256];
char *bufptr;
int nbytes;

int sockfd, newsockfd, portno;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

 
// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
void signal_callback_handler(int signum) {
	printf("Caught signal %d\n",signum);

	struct MHD_Daemon *daemon;
	MHD_stop_daemon (daemon);

	// Terminate program
	exit(signum);
}

void kill_power_led() {
	FILE *fp;
	fp = fopen(POWER_LED_FILE, "w");
	fprintf(fp, "0\n");
	fclose(fp);
}

int open_serial_port() {
		int c, res;
		struct termios oldtio,newtio;
		char buf[255];
/* 
		Open modem device for reading and writing and not as controlling tty
		because we don't want to get killed if linenoise sends CTRL-C.
*/
	serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY ); 
	if (serial_fd <0) {perror(SERIAL_PORT); exit(-1); }

	tcgetattr(serial_fd,&oldtio); /* save current serial port settings */
	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */

/* 
		BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
		CRTSCTS : output hardware flow control (only used if the cable has
												all necessary lines. See sect. 7 of Serial-HOWTO)
		CS8     : 8n1 (8bit,no parity,1 stopbit)
		CLOCAL  : local connection, no modem contol
		CREAD   : enable receiving characters
*/
	newtio.c_cflag = BAUDRATE | HUPCL | CS8 | CLOCAL | CREAD;
	
/*
		IGNPAR  : ignore bytes with parity errors
		ICRNL   : map CR to NL (otherwise a CR input on the other computer
												will not terminate input)
		otherwise make device raw (no other input processing)
*/
	newtio.c_iflag &= ~( ICRNL | IGNPAR );
	
/*
	Raw output.
*/
	newtio.c_oflag = 0;
	
/*
		ICANON  : enable canonical input
		disable all echo functionality, and don't send signals to calling program
*/
	newtio.c_lflag &= ~( ICANON );
	
/* 
		initialize all control characters 
		default values can be found in /usr/include/termios.h, and are given
		in the comments, but we don't need them here
*/
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */

/* 
		now clean the modem line and activate the settings for the port
*/
	tcflush(serial_fd, TCIFLUSH);
	tcsetattr(serial_fd,TCSANOW,&newtio);

/*
		terminal settings done, now handle input
		In this example, inputting a 'z' at the beginning of a line will 
		exit the program.
*/
	/* restore the old port settings */
	//tcsetattr(serial_fd,TCSANOW,&oldtio);
}



void *get_url(void *url) {
	CURL *curl;
	CURLcode res;
	FILE *devnull;

	
	curl = curl_easy_init();
	if(curl) {
		devnull = fopen("/dev/null", "w");

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Alis Home Automation Network Server v0.1");
		res = curl_easy_perform(curl);

		fclose(devnull);
		/* always cleanup */ 
		curl_easy_cleanup(curl);
		return NULL;
	}
	return 0;
}

int powermeter() {
	pthread_t tid[NUM_INSTALLED_COUNTERS];
	int t_error;
	int counter[NUM_COUNTERS];
	uint32_t duration[NUM_COUNTERS];
	char urls[NUM_INSTALLED_COUNTERS][255];
	int rf12_hdr;
	int rf12_seq;

	if (sscanf(serial_buffer, "hdr: 0x%x, seq: %d, data = %d %d %d %d %d %d %d %d %d %d", &rf12_hdr, &rf12_seq, &counter[COUNTER1], &duration[COUNTER1], &counter[COUNTER2], &duration[COUNTER2], &counter[COUNTER3], &duration[COUNTER3], &counter[COUNTER4], &duration[COUNTER4], &counter[COUNTER5], &duration[COUNTER5]) == 12) {

#ifdef COUNTER1_URL
		sprintf(urls[0], COUNTER1_URL, counter[COUNTER1]);
#endif
#ifdef COUNTER2_URL
		sprintf(urls[1], COUNTER2_URL, counter[COUNTER2]);
#endif
#ifdef COUNTER3_URL
		sprintf(urls[2], COUNTER3_URL, counter[COUNTER3]);
#endif
#ifdef COUNTER4_URL
		sprintf(urls[3], COUNTER3_URL, counter[COUNTER4]);
#endif
#ifdef COUNTER5_URL
		sprintf(urls[4], COUNTER3_URL, counter[COUNTER5]);
#endif

		for (int i = 0 ; i < NUM_INSTALLED_COUNTERS ; i++) {
			t_error = pthread_create(&tid[i], NULL, get_url, (void *)urls[i]);
			if (0 != t_error) {
				fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, t_error);
			} else {
				//fprintf(stderr, "Thread %d, gets %s\n", i, urls[i]);
				pthread_detach(tid[i]);
			}
		}
	}
	return(0);
}

int thermometer() {
	pthread_t tid[1];
	int t_error;
	float tC;
	int t_awake;
	char url[255];
	int rf12_hdr;
	int rf12_seq;

	if (sscanf(serial_buffer, "hdr: 0x%x, seq: %d, data = %f degrees C, awake: %dms", &rf12_hdr, &rf12_seq, &tC, &t_awake) == 4) {
		sprintf(url, "http://lardass/middleware.php/data/8fa9d0f0-6af2-11e1-a30d-f73d9a35e2de.json?operation=add&value=%.2f", tC);

		t_error = pthread_create(&tid[0], NULL, get_url, (void *)url);
		if (0 != t_error) {
			fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);
		} else {
			//fprintf(stderr, "Thread %d, gets %s\n", 0, url);
			pthread_detach(tid[0]);
		}
	}
	return(0);
}

void *read_stdin(void *null){

	int strIdx;
	char inChar;
	char stdin_string[256];

	while (1) {
		if(strIdx < 255) {
			inChar = getc (stdin);
			stdin_string[strIdx++] = inChar;
			if ((inChar == '\n') || (inChar == '\r')) {
				write(serial_fd, stdin_string, strIdx);
				memset(&stdin_string[0], 0, sizeof(stdin_string));
				strIdx = 0;
			}
		} else {
			memset(&stdin_string[0], 0, sizeof(stdin_string));
			strIdx = 0;
		}
	}
}

void *read_serial(void *null) {
	open_serial_port();
	while (1) {
		/* read characters into our string buffer until we get a CR or NL */
		bufptr = serial_buffer;
		while ((nbytes = read(serial_fd, bufptr, serial_buffer + sizeof(serial_buffer) - bufptr - 1)) > 0) {
			bufptr += nbytes;
			if (bufptr[-1] == '\n' || bufptr[-1] == '\r') {

				int bufsize = bufptr - serial_buffer;
				char http_data[bufsize];
				strncpy(http_data, serial_buffer, bufsize -2);
				
				sprintf(page, "<html><body>%s</body></html>", http_data);
				//fprintf(stderr, "bufsize: %d\tbufptr: %d\tserial_buffer: %d\t<html><body>%s</body></html>\n", bufsize, bufptr, serial_buffer, http_data);

				fprintf(stderr, "%s", serial_buffer);
				int rf12_hdr;
				if (sscanf(serial_buffer, "hdr: 0x%x", &rf12_hdr) == 1) {
					if (rf12_hdr == 0x25) {
						powermeter();
					} else if (rf12_hdr == 0x2d) {
						thermometer();
					//} else if (sscanf(serial_buffer, "Socket State:%c") == 1) {
					////	thermometer();
					}
				}
				memset(&serial_buffer[0], 0, sizeof(serial_buffer));
				memset(&http_data[0], 0, sizeof(http_data));
				break;
			}
		}
	}
	close(serial_fd);
	return(0);
}

void *read_socket(void *null) {
	portno = TCP_PORT;
	socklen_t clilen;
	char socket_buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) error("ERROR opening socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");

	while (1) {
		listen(sockfd,5);
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, 
														(struct sockaddr *) &cli_addr, 
														&clilen);
		if (newsockfd < 0) error("ERROR on accept");

		bzero(socket_buffer,256);
		n = read(newsockfd,socket_buffer,255);
		if (n < 0) error("ERROR reading from socket");

		printf("Here is the message: %s, size: %d\n", socket_buffer, n);
		//n = write(newsockfd, webpage, sizeof(webpage));

		//if (socket_buffer[n-1] == '\n' || socket_buffer[n-1] == '\r') {
			n = write(serial_fd, socket_buffer, n);
		//}


		if (n < 0) error("ERROR writing to socket");

		close(newsockfd);
	}
	close(sockfd);
	return 0; 
}

int print_out_key (void *cls, enum MHD_ValueKind kind, 
                   const char *key, const char *value)
{
  printf ("%s: %s\n", key, value);
  return MHD_YES;
}

int httpd_callback (void *cls, struct MHD_Connection *connection, 
                          const char *url, 
                          const char *method, const char *version, 
                          const char *upload_data, 
                          size_t *upload_data_size, void **con_cls)
{

	struct MHD_Response *response;
	int ret;

	printf ("New %s request for %s (length: %d) using version %s\n", method, url, sizeof(url), version);
	//MHD_get_connection_values (connection, MHD_HEADER_KIND, &print_out_key, NULL);


	if ((strncmp(url, "/\0", 2)  == 0) || (strncmp(url, "/index.html\0", 12) == 0)) {
		fprintf(stderr, "INDEX!\n");
		FILE *fp;
		fp = fopen(INDEX_HTML, "r");
		fclose(fp);
	}
	int strIdx;
	char serial_string[255];
	for (strIdx = 0; strIdx < 254 ; strIdx++){
		if (url[strIdx] == '\0') {
			serial_string[strIdx] = '\n';
			break;
		} else {
			serial_string[strIdx] = url[strIdx];
		}
	}

	printf("url index: %d\n", strIdx);
	if (strncmp(serial_string, "/favicon.ico", 12) != 0) { // requests for /favicon.ico are ignored
		write(serial_fd, serial_string, strIdx + 1);
	}

	int response_timer = 0;
	memset(&page[0], 0, sizeof(page));
	while(strlen(page) <= 0) {
		//printf("waiting for response\n");
		sleep(1);
		response_timer++;
	}
	fprintf(stderr, "Spent %dms waiting for response\n", response_timer);
	
	response = MHD_create_response_from_data(strlen(page), (void*)page, 0, 0);
 ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);

	return ret;


	//return MHD_NO;
}







int main(void) {
	pthread_t tid[2];
	int t_error;
	struct MHD_Daemon *daemon;

	// Register signal and signal handler
	signal(SIGINT, signal_callback_handler);

	kill_power_led();

	// start serial listener
	t_error = pthread_create(&tid[0], NULL, read_serial, NULL);
	fprintf(stderr, "Starting serial read thread\n");
	if (0 != t_error) 
		fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);

	// start stdin_listener
	t_error = pthread_create(&tid[1], NULL, read_stdin, NULL);
	fprintf(stderr, "Starting stdin read thread\n");
	if (0 != t_error) 
		fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 1, t_error);

	// start http daemon
	fprintf(stderr, "Starting httpd daemon\n");
	daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, HTTPD_PORT, NULL, NULL, 
                             &httpd_callback, NULL, MHD_OPTION_END);
	if (NULL == daemon) error("ERROR starting httpd");


	// ..join a thread until backgrounding is implemented

	pthread_join(tid[0], NULL);


	return 0;
}


/*
// configuration, as stored in EEPROM
struct Config {
  uint8_t band;
  uint8_t group;
  uint8_t collect;
  uint16_t port;
  uint8_t valid; // keep this as last byte
} config;

// ethernet interface mac address - must be unique on your network
static uint8_t mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

// buffer for an outgoing data packet
//static uint8_t outBuf[RF12_MAXDATA], outDest;
static uint8_t outCount = -1;

// this buffer will be used to construct a collectd UDP packet
static uint8_t collBuf [200], collPos;

#define NUM_MESSAGES  3     // Number of messages saved in history
#define MESSAGE_TRUNC 15    // Truncate message payload to reduce memory use

//static BufferFiller bfill;  // used as cursor while filling the buffer

static uint8_t history_rcvd[NUM_MESSAGES][MESSAGE_TRUNC+1]; //history record
static uint8_t history_len[NUM_MESSAGES]; // # of RF12 messages+header in history
static uint8_t next_msg;       // pointer to next rf12rcvd line
static uint16_t msgs_rcvd;      // total number of lines received modulo 10,000

// byte Ethernet::buffer[700];   // tcp/ip send and receive buffer

static void loadConfig () {
    config.valid = 253;
    config.band = 8;
    config.group = 1;
    config.collect = 1;
    config.port = 25827;
  uint8_t freq = config.band == 4 ? RF12_433MHZ :
              config.band == 8 ? RF12_868MHZ :
                                 RF12_915MHZ;
}



static void collectTypeLen (uint16_t type, uint16_t len) {
  len += 4;
  collBuf[collPos++] = type >> 8;
  collBuf[collPos++] = (uint8_t) type;
  collBuf[collPos++] = len >> 8;
  collBuf[collPos++] = (uint8_t) len;
}

static void collectStr (uint16_t type, const uint8_t* data) {
  uint16_t len = strlen(data) + 1;
  collectTypeLen(type, len);
  strcpy((uint8_t*) collBuf + collPos, data);
  collPos += len;
}

static void collectPayload (uint16_t type) {
  // Copy the received RF12 data into a as many values as needed.
  uint8_t num = rf12_len / 8 + 1; // this many values will be needed
  collectTypeLen(type, 2 + 9 * num);
  collBuf[collPos++] = 0;
  collBuf[collPos++] = num;
  for (uint8_t i = 0; i < num; ++i)
    collBuf[collPos++] = 0; // counter
  for (uint8_t i = 0; i < 8 * num; ++i) // include -1, i.e. the length byte
    collBuf[collPos++] = i <= rf12_len ? rf12_data[i-1] : 0;
}

static void forwardToUDP () {
  static uint8_t destIp[] = { 239,192,74,66 }; // UDP multicast address
  uint8_t buf[10];
  
  collPos = 0;
  collectStr(0x0000, "JeeUdp");
  collectStr(0x0002, "RF12");
  uint16_t mhz = config.band == 4 ? 433 : config.band == 8 ? 868 : 915;
  sprintf(buf, "%d.%d", mhz, config.group);
  collectStr(0x0003, buf);
  collectStr(0x0004, "OK");
  sprintf(buf, "%d", rf12_hdr);
  collectStr(0x0005, buf);
  collectPayload(0x0006);
  
  ether.sendUdp ((uint8_t*) collBuf, collPos, 23456, destIp, config.port);
#if SERIAL
  Serial.println("UDP sent");
#endif
}

*/

