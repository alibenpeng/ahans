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
#include <sys/stat.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <curl/curl.h>
#include <microhttpd.h>

#define SERIAL_PORT "/dev/ttyS1"
#define BAUDRATE B57600

#define POWER_LED_FILE "/proc/diag/led/power"

#define HTTPD_PORT 8888
#define HTTPD_ROOT "/www/ahans"

#define NUM_COUNTERS 5
#define NUM_INSTALLED_COUNTERS 3
#define COUNTER1 0
#define COUNTER2 1
#define COUNTER3 2
#define COUNTER4 3
#define COUNTER5 4
#define COUNTER1_URL "http://yavdr:81/middleware.php/data/46b27ba0-6ae1-11e1-91d0-4315961506a9.json?operation=add&value=%d"
#define COUNTER2_URL "http://yavdr:81/middleware.php/data/60435230-6ae1-11e1-bc8b-7f8bbcdebe7a.json?operation=add&value=%d"
#define COUNTER3_URL "http://yavdr:81/middleware.php/data/65c072e0-6ae1-11e1-879a-7bc14050b5a0.json?operation=add&value=%d"

#define MAX_STRINGS 10
#define FALSE 0
#define TRUE 1

typedef struct {
	char url[256];
	char data[256];
} put_args_t;

int msgid;
int avr_failure;

int num_strings;
char serial_strings[MAX_STRINGS][256];

static char empty_page[65535];
static char *page = empty_page;
bool page_delivered = 1;

int waiting_for_response;

int serial_fd; /* File descriptor for the serial port */

void error(const char *msg) {
    perror(msg);
    exit(1);
}

char *timestamp() {
	time_t ltime;
	struct tm *Tm;
	static char ts[22];
 
	ltime=time(NULL);
	Tm=localtime(&ltime);

	sprintf(ts, "%04d-%02d-%02dT%02d:%02d:%02d",
									Tm->tm_year + 1900,
									Tm->tm_mon,
									Tm->tm_mday,
									Tm->tm_hour,
									Tm->tm_min,
									Tm->tm_sec);
	return (char *)ts;
}
 
// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
void signal_callback_handler(int signum) {
	printf("Caught signal %d\n",signum);

/*
	struct MHD_Daemon *daemon;
	MHD_stop_daemon (daemon);
*/

	// Terminate program
	exit(signum);
}

void kill_power_led() {
	FILE *fp;
	fp = fopen(POWER_LED_FILE, "w");
	fprintf(fp, "0\n");
	fclose(fp);
}

void *put_rest_data(void *data) {
	CURL *curl;
	CURLcode res;
	FILE *devnull;
	put_args_t put_args;
	put_args = *(put_args_t*)  data;

	printf("PUTting Data %s to URL %s\n", put_args.data, put_args.url);
	
	//printf("GET_URL: Getting URL: %s\n", url);
	curl = curl_easy_init();
	if(curl) {
		devnull = fopen("/dev/null", "w");

		struct curl_slist *headers=NULL;
		headers = curl_slist_append(headers, "Content-Type: text/plain");

		curl_easy_setopt(curl, CURLOPT_URL, put_args.url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
		//curl_easy_setopt(curl, CURLOPT_RETURNTRANSFER, TRUE);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Alis Home Automation Network Server v0.1");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, put_args.data);
		res = curl_easy_perform(curl);

		fclose(devnull);
		/* always cleanup */ 
		curl_easy_cleanup(curl);
		return NULL;
	}
	return 0;
}


void *get_url(void *url) {
	CURL *curl;
	CURLcode res;
	FILE *devnull;

	
	//printf("GET_URL: Getting URL: %s\n", url);
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

int powermeter(char *buffer) {
	pthread_t tid[NUM_INSTALLED_COUNTERS];
	int t_error;
	int counter[NUM_COUNTERS];
	uint32_t duration[NUM_COUNTERS];
	static char urls[NUM_INSTALLED_COUNTERS][256];
	int rf12_node_id;
	int rf12_seq;

	//printf("POWERMETER: %s\n", buffer);
	if (sscanf(buffer, "Node ID: %d, seq: %d, data = %d %d %d %d %d %d %d %d %d %d", &rf12_node_id, &rf12_seq, &counter[COUNTER1], &duration[COUNTER1], &counter[COUNTER2], &duration[COUNTER2], &counter[COUNTER3], &duration[COUNTER3], &counter[COUNTER4], &duration[COUNTER4], &counter[COUNTER5], &duration[COUNTER5]) == 12) {

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
		sprintf(urls[3], COUNTER4_URL, counter[COUNTER4]);
#endif
#ifdef COUNTER5_URL
		sprintf(urls[4], COUNTER5_URL, counter[COUNTER5]);
#endif

		for (int i = 0 ; i < NUM_INSTALLED_COUNTERS ; i++) {
			//printf("POWERMETER: Getting URL: %s\n", urls[i]);
			//get_url(urls[1]);
			t_error = pthread_create(&tid[i], NULL, get_url, (void *)urls[i]);
			if (0 != t_error) {
				fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, t_error);
			} else {
				pthread_detach(tid[i]);
			}
/*
*/
		}
	}
	return(0);
}

int thermometer(char *buffer) {
	pthread_t tid[2];
	int t_error;
	float tC;
	int t_awake;
	//static char urls[2][256];
	static char url[256];
	int rf12_node_id;
	int rf12_seq;
	static put_args_t put_args;

	if (sscanf(buffer, "Node ID: %d, seq: %d, data = %f degrees C, awake: %dms", &rf12_node_id, &rf12_seq, &tC, &t_awake) == 4) {
		sprintf(url, "http://yavdr:81/middleware.php/data/8fa9d0f0-6af2-11e1-a30d-f73d9a35e2de.json?operation=add&value=%.2f", tC);
		sprintf(put_args.url, "http://yavdr:8081/rest/items/Temperature_FF_Office/state");
		sprintf(put_args.data, "%.2f", tC);

/*
		sprintf(urls[0], "http://yavdr:81/middleware.php/data/8fa9d0f0-6af2-11e1-a30d-f73d9a35e2de.json?operation=add&value=%.2f", tC);
		sprintf(urls[1], "http://yavdr:8081/", counter[COUNTER5]);

		for (int i = 0 ; i < 2 ; i++) {
			t_error = pthread_create(&tid[i], NULL, get_url, (void *)urls[i]);
			if (0 != t_error) {
				fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, t_error);
			} else {
				pthread_detach(tid[i]);
			}
		}
*/
		t_error = pthread_create(&tid[0], NULL, get_url, (void *)url);
		if (0 != t_error) {
			fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);
		} else {
			//fprintf(stderr, "Thread %d, gets %s\n", 0, url);
			pthread_detach(tid[0]);
		}

		t_error = pthread_create(&tid[1], NULL, put_rest_data, &put_args);
		if (0 != t_error) {
			fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);
		} else {
			//fprintf(stderr, "Thread %d, gets %s\n", 0, url);
			pthread_detach(tid[1]);
		}

	}
	return(0);
}

int powersocket(char *buffer) {
	pthread_t tid[1];
	int t_error;
	int rf12_node_id;
	int rf12_seq;
	int socket_group, socket_id, socket_state = 0;
	static put_args_t put_args;

// {"socket_state": [ "group": "8", "id": "1", "state": "1" ] };
	if (sscanf(buffer, "Node ID: %d, seq: %d, data = {\"socket_state\": [ \"group\": \"%d\", \"id\": \"%d\", \"state\": \"%d\" ] };", &rf12_node_id, &rf12_seq, &socket_group, &socket_id, &socket_state) == 5) {

		sprintf(put_args.url, "http://yavdr:8081/rest/items/Socket_%d_%d/state");
		if (socket_state) {
			put_args.state = "ON";
		}

		t_error = pthread_create(&tid[0], NULL, put_rest_data, &put_args);
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
				// write input string to serial port
				fprintf(stderr, "READ_STDIN: writing stdin string to serial fd\n");
				write(serial_fd, stdin_string, strIdx);
				fprintf(stderr, "READ_STDIN: finished writing stdin string to serial fd\n");
				fprintf(stderr, "READ_STDIN: clearing stdin string\n");
				memset(&stdin_string[0], 0, sizeof(stdin_string));
				strIdx = 0;
			}
		} else {
			fprintf(stderr, "READ_STDIN: stdin string too long - clearing stdin string\n");
			memset(&stdin_string[0], 0, sizeof(stdin_string));
			strIdx = 0;
		}
	}
}

void open_serial_port() {
		struct termios tio;

/* 
		Open modem device for reading and writing and not as controlling tty
		because we don't want to get killed if linenoise sends CTRL-C.
*/
	serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY ); 
	if (serial_fd <0) {perror(SERIAL_PORT); exit(-1); }

	bzero(&tio, sizeof(tio)); /* clear struct for new port settings */

	tio.c_cflag = BAUDRATE | HUPCL | CS8 | CLOCAL | CREAD;
	tio.c_iflag = IGNPAR;
	tio.c_iflag &= ~( ICRNL  );
	tio.c_oflag = 0;
	tio.c_lflag = ICANON;
	//tio.c_lflag &= ~( ICANON );
	
/* 
		initialize all control characters 
		default values can be found in /usr/include/termios.h, and are given
		in the comments, but we don't need them here
*/
	tio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	tio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	tio.c_cc[VERASE]   = 0;     /* del */
	tio.c_cc[VKILL]    = 0;     /* @ */
	tio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	tio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	tio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
	tio.c_cc[VSWTC]    = 0;     /* '\0' */
	tio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	tio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	tio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	tio.c_cc[VEOL]     = 0;     /* '\0' */
	tio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	tio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	tio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	tio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	tio.c_cc[VEOL2]    = 0;     /* '\0' */

		// now clean the modem line and activate the settings for the port
	tcflush(serial_fd, TCIFLUSH);
	tcsetattr(serial_fd,TCSANOW,&tio);
}

void *serial_read(void *null) {
	int res;
	char buf[256];
	char http_string[256];
	int rcvd_msgid;

 while (1) {
         /* read blocks program execution until a line terminating character is 
            input, even if more than 255 chars are input. If the number
            of characters read is smaller than the number of chars available,
            subsequent reads will return the remaining chars. res will be set
            to the actual number of characters actually read */
		//fprintf(stderr, "SERIAL_READ: Beginning serial read\n");
		res = read(serial_fd,buf,255); 
		//fprintf(stderr, "SERIAL_READ: Finished serial read\n");
		buf[res]=0;             /* set end of string, so we can printf */
		//printf("DATA: %s\nSize:%d\n", buf, res);
		if ((buf[res-1] == '\n') || (buf[res-1] == '\r')) {
			//fprintf(stderr, "SERIAL_READ: Found Linebreak: %d\n", buf[res-1]);

			fprintf(stderr, "SERIAL_READ: %s: %s", timestamp(), buf);

				if (waiting_for_response == 1) {
					if (sscanf(buf, "MSG ID: 0x%x: %[^;]", &rcvd_msgid, &http_string) == 2) {
						if (rcvd_msgid == msgid) {
							sprintf(page, "%s", http_string);
							fprintf(stderr, "SERIAL_READ: wrote \"%s\" to html page\n", page);
						} else {
							sprintf(page, "<html><body></body></html>");
							fprintf(stderr, "SERIAL_READ: msgid mismatch: 0x%x != 0x%x: wrote empty page\n", msgid, rcvd_msgid);
						}
						waiting_for_response = 0;
					}
				}
/*

			if (num_strings < MAX_STRINGS) {
				memset(&serial_strings[num_strings][0], 0, sizeof(serial_strings[num_strings]));
				strncpy(serial_strings[num_strings++], buf, res);
			} else {
				num_strings = 0;
			}
*/
			int rf12_node_id;
			if (sscanf(buf, "Node ID: %d", &rf12_node_id) == 1) {
				if (rf12_node_id == 5) {
					powermeter(buf);
				} else if (rf12_node_id == 6) {
					thermometer(buf);
				} else if (rf12_node_id == 8) {
					powersocket(buf);
				}
			}
		}
	}
}


int parse_get_args (void *cls, enum MHD_ValueKind kind, 
                  const char *key, const char *value) {
  printf ("PARSE_GET_ARGS: %s: %s\n", key, value);

		int strIdx;
		char serial_string[256];
		int sstring_size = snprintf(serial_string, 256, "MSG ID: 0x%04x: %s=%s\n", msgid, key, value);

		int response_timeout = 2;

		page_delivered = 0;

		printf("PARSE_GET_ARGS: sending %s (size: %d) to the serial port\n", serial_string, sstring_size);
		write(serial_fd, serial_string, sstring_size);

		printf("PARSE_GET_ARGS: waiting for response from avr\n");
		waiting_for_response = 1;
		while(strlen(page) <= 0) {
			if (response_timeout-- <= 0) {
				fprintf(stderr, "HTTP_CALLBACK: ERROR! No response from AVR after %d seconds\n", response_timeout);
				avr_failure = 1;
				return false;
			}
			sleep(1);
		}
		fprintf(stderr, "PARSE_GET_ARGS: Spent %d seconds waiting for response from avr\n", response_timeout);
		msgid++;
}

int httpd_callback (void *cls, struct MHD_Connection *connection, 
																				const char *url, 
																				const char *method, const char *version, 
																				const char *upload_data, 
																				size_t *upload_data_size, void **con_cls) {

	struct MHD_Response *response;
	int ret, timeout;

	while(! page_delivered) {
		if (avr_failure == 1) {
			waiting_for_response = 0;
			page_delivered = 1;
			return MHD_NO;
		}
		printf("HTTPD_CALLBACK: Fucking waiting for page to be delivered\n");
		sleep(1);
	}

	printf ("HTTPD_CALLBACK: Starting HTTP Callback for new %s request for %s using version %s\n", method, url, version);

	memset(&page[0], 0, 65535);
	if (! MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, &parse_get_args, NULL)) {
		if (strncmp(url, "/\0", 2) == 0) {
			url = "/index.html";
		}

		char httpd_file_path[256] = HTTPD_ROOT;
		strncat(httpd_file_path, url, 256);
		

		struct stat sb;
		if (stat(httpd_file_path, &sb) == -1) {
			fprintf(stderr, "HTTPD_CALLBACK: stat error: %s\n", httpd_file_path);
			return MHD_NO;
		}
		
		if ((sb.st_mode & S_IFMT) == S_IFREG) {
			fprintf(stderr, "HTTPD_CALLBACK: delivering %s\n", httpd_file_path);
			int pageIdx = 0;
			char inChar;
			FILE *fp;
			fp = fopen(httpd_file_path, "r");
			while (inChar != EOF) {
				inChar = fgetc(fp);
				if (inChar != EOF) {
					page[pageIdx++] = inChar;
				}
			}
			fclose(fp);
		} else {
			// 404
			fprintf(stderr, "HTTPD_CALLBACK: not a regular file: %s 0x%x != 0x%x\n", httpd_file_path, sb.st_mode & S_IFMT, S_IFREG);
			return MHD_NO;
		}
	}

	printf("HTTPD_CALLBACK: delivering page\n");

	response = MHD_create_response_from_data(strlen(page), (void*)page, 0, 0);
 ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);

	page_delivered = 1;
	return ret;
}







int main(void) {
	pthread_t tid[2];
	int t_error;
	struct MHD_Daemon *daemon;

	// Register signal and signal handler
	signal(SIGINT, signal_callback_handler);

	open_serial_port();
	kill_power_led();

	// start serial listener
	t_error = pthread_create(&tid[0], NULL, serial_read, NULL);
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
	//daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, HTTPD_PORT, NULL, NULL, 
	daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, HTTPD_PORT, NULL, NULL, 
                             &httpd_callback, NULL, MHD_OPTION_END);
	if (NULL == daemon) error("ERROR starting httpd");


	// ..join a thread until backgrounding is implemented

	pthread_join(tid[0], NULL);


	return 0;
}


