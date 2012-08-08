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
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <curl/curl.h>
#include <microhttpd.h>

#include "config.h"

<<<<<<< HEAD
#define GET             0
#define POST            1


=======

>>>>>>> c3b7a1e7f58601fb4559db8673dcf4719e5c6b2e
int serial_fd; /* File descriptor for the serial port */
int msgid = 0;
char answer_strings[MAX_ANSWERSTRINGS][MAX_ANSWERSTRING_LENGTH];

typedef struct {
	char method[4];
	char url[256];
	char data[256];
} curl_args_t;

struct connection_info_struct {
  int connectiontype;
  char *answerstring;
  struct MHD_PostProcessor *postprocessor;
};

const char *answerpage = "%s";
const char *errorpage = "<html><body>This doesn't seem to be right.</body></html>";


// Generic functions /////////////////////////////////////////////////////////////

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
	exit(signum);
}


void kill_power_led() {
	FILE *fp;
	fp = fopen(POWER_LED_FILE, "w");
	if (fp) {
		fprintf(fp, "0\n");
		fclose(fp);
	}
}



void *curl_rest(void *data) {
	CURL *curl;
	//CURLcode res;
	FILE *devnull;
	const curl_args_t curl_args = *(curl_args_t*) data;

	if (NULL == curl_args.data)
		printf("curl_rest(): method: \"%s\", url: \"%s\"\n", curl_args.method, curl_args.url);
	else
		printf("curl_rest(): method: \"%s\", data: \"%s\", url: \"%s\"\n", curl_args.method, curl_args.data, curl_args.url);
	
	curl = curl_easy_init();
	if(curl) {
		struct curl_slist *headers=NULL;
		devnull = fopen("/dev/null", "w");

		if (0 == strncmp(curl_args.method, "GET", 3)) {
			printf("curl_rest(): Getting URL: \"%s\"\n", curl_args.url);
		} else if (0 == strncmp(curl_args.method, "PUT", 3)) {
			printf("curl_rest(): PUTting Data \"%s\" to URL \"%s\"\n", curl_args.data, curl_args.url);
			headers = curl_slist_append(headers, "Content-Type: text/plain");
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, curl_args.data);
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		}

		curl_easy_setopt(curl, CURLOPT_URL, curl_args.url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Alis Home Automation Network Server v0.1");
		//res = curl_easy_perform(curl);
		curl_easy_perform(curl);

		fclose(devnull);
		// always cleanup
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return NULL;
	}
	return 0;
}

int curl_helper(char *buf) {
	//pthread_t tid[1];
	//int t_error;
	curl_args_t curl_args;
	memset(&curl_args, 0, sizeof curl_args);
	//curl_args_t *curl_args;
	//curl_args = (curl_args_t*) malloc(sizeof(curl_args_t));
	//memset(curl_args, 0, sizeof(curl_args_t));

	printf("curl_helper(): Processing \"%s\"\n", buf);
	if (sscanf(buf, "REST %[^:]: URL{%[^}]}, DATA{%[^}]}", curl_args.method, curl_args.url, curl_args.data) == 3) {
		printf("curl_helper(): calling curl_rest() with args method: \"%s\", data: \"%s\", url: \"%s\"\n", curl_args.method, curl_args.data, curl_args.url);
		//strcpy(curl_args.method, "PUT");
	} else if (sscanf(buf, "REST GET: URL{%[^}]}", curl_args.data) == 1) {
		printf("curl_helper(): GET\n");
		strcpy(curl_args.method, "GET");
		//curl_args.data = "";
	}
<<<<<<< HEAD

/*
	t_error = pthread_create(&tid[0], NULL, curl_rest, &curl_args);
	if (0 != t_error) {
		fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);
	} else {
		pthread_detach(tid[0]);
	}
*/
	curl_rest(&curl_args);
	//free(curl_args);
	return(0) ;
}


char *send_serial_cmd (struct MHD_Connection *connection, const char *cmd_string) {

	int response_timeout = 2000; // in milliseconds!
	int rcvd_msgid;
	char serial_string[256];
	char serial_answer[256];

	if (msgid >= 0xffff) msgid = 0;
	int sstring_size = snprintf(serial_string, 256, "MSG ID: 0x%04x: %s\n", ++msgid, cmd_string);

	// Send string to serial port and wait for response!
	printf("send_serial_cmd(): sending %s (size: %d) to the serial port\n", serial_string, sstring_size);
	write(serial_fd, serial_string, sstring_size);

	printf("send_serial_cmd(): waiting for answer from AVR\n");
	while (response_timeout >= 0) {
		// search in answer_strings array for msgid
		for (int i = 0 ; i <= (MAX_ANSWERSTRINGS -1) ; i++) {
			if (sscanf(answer_strings[i], "MSG ID: 0x%x: %s", &rcvd_msgid, serial_answer) == 2) {
				//printf("send_serial_cmd(): Found string: %s\n", answer_strings[i]);
				// array elemt with msgid found - check if it is the msgid we are looking for!
				if (rcvd_msgid == msgid) {
					// we found the right msgid
					printf("send_serial_cmd(): Found MSG ID: 0x%04x\n", msgid);
					return(answer_strings[i]);
				}
			}
		}
		response_timeout--;
		usleep(1000);
	}
	printf("send_serial_cmd(): No answer from AVR for MSG ID 0x%04x - sending error page\n", msgid);
	return (char *) "ERROR";
}




int send_page (struct MHD_Connection *connection, const char *page) {
  int ret;
  struct MHD_Response *response;


  response = MHD_create_response_from_data (strlen (page), (void *) page, MHD_NO, MHD_NO);
  if (!response)
    return MHD_NO;

		printf("send_page(): Queueing %s\n", page);
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}



int process_get_request (struct MHD_Connection *connection, const char *url, void *coninfo_cls) {

	//int item_group, item_id = 0;
	//char item_name, item_request[32];
	struct connection_info_struct *con_info = (struct connection_info_struct *) coninfo_cls;

	//if (sscanf(url, "/%s/%d/%d/%s", &item_name, &item_group, &item_id, &item_request) == 4) {
	//	printf("Processing HTTP GET Request: Item: %s, Group: %d, ID: %d, Command: %s\n", item_name, item_group, item_id, item_request);
	//}
	char *answerstring;
	answerstring = (char*) malloc (MAXANSWERSIZE);
	if (!answerstring) return MHD_NO;

	char *data = send_serial_cmd(connection, url);
	snprintf (answerstring, MAXANSWERSIZE, answerpage, data);
	con_info->answerstring = answerstring;
	printf("Returning %s\n", con_info->answerstring);
	//free(answerstring);
	return MHD_NO;
}


int
iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size)
{
  //struct connection_info_struct *con_info = (struct connection_info_struct *) coninfo_cls;

/*

  if ((0 == strncmp (key, "ON", 3)) || (0 == strncmp (key, "OFF", 3))) {

// print command to serial port and return!

      if ((size > 0) && (size <= MAXNAMESIZE))
        {
          char *answerstring;
          answerstring = (char*) malloc (MAXANSWERSIZE);
          if (!answerstring)
            return MHD_NO;

          snprintf (answerstring, MAXANSWERSIZE, answerpage, data);
          con_info->answerstring = answerstring;
        }
      else
        con_info->answerstring = NULL;

      return MHD_NO;
    }
*/

  return MHD_YES;
}

void request_completed (void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
	struct connection_info_struct *con_info = (struct connection_info_struct *) *con_cls;

	if (NULL == con_info) return;

	if (con_info->connectiontype == POST) {
		MHD_destroy_post_processor (con_info->postprocessor);
	}
	if (con_info->answerstring) free (con_info->answerstring);

	free (con_info);
	*con_cls = NULL;
}


int answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls) {

	if (NULL == *con_cls) {
		printf("answer_to_connection(): *con_cls == NULL\n");
		struct connection_info_struct *con_info;

		con_info = (connection_info_struct*) malloc (sizeof (struct connection_info_struct));

		if (NULL == con_info) return MHD_NO;

		con_info->answerstring = NULL;

		if (0 == strncmp (method, "POST", 4)) {
			con_info->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void *) con_info);

			if (NULL == con_info->postprocessor) {
				free (con_info);
				return MHD_NO;
			}

=======

/*
	t_error = pthread_create(&tid[0], NULL, curl_rest, &curl_args);
	if (0 != t_error) {
		fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);
	} else {
		pthread_detach(tid[0]);
	}
*/
	curl_rest(&curl_args);
	//free(curl_args);
	return(0) ;
}


char *send_serial_cmd (struct MHD_Connection *connection, const char *cmd_string) {

	int response_timeout = 1000; // in milliseconds!
	int rcvd_msgid;
	char serial_string[256];
	char serial_answer[256];

	int sstring_size = snprintf(serial_string, 256, "MSG ID: 0x%04x: %s\n", ++msgid, cmd_string);

	// Send string to serial port and wait for response!
	printf("send_serial_cmd(): sending %s (size: %d) to the serial port\n", serial_string, sstring_size);
	write(serial_fd, serial_string, sstring_size);

	printf("send_serial_cmd(): waiting for answer from AVR\n");
	while (response_timeout >= 0) {
		// search in answer_strings array for msgid
		for (int i = 0 ; i <= (MAX_ANSWERSTRINGS -1) ; i++) {
			if (sscanf(answer_strings[i], "MSG ID: 0x%x: %s", &rcvd_msgid, serial_answer) == 2) {
				//printf("send_serial_cmd(): Found string: %s\n", answer_strings[i]);
				// array elemt with msgid found - check if it is the msgid we are looking for!
				if (rcvd_msgid == msgid) {
					// we found the right msgid
					printf("send_serial_cmd(): Found MSG ID: %d\n", msgid);
					return(answer_strings[i]);
				}
			}
		}
		response_timeout--;
		usleep(1000);
	}
	printf("send_serial_cmd(): No answer from AVR - sending error page\n");
	return (char *) "ERROR";
}




int send_page (struct MHD_Connection *connection, const char *page) {
  int ret;
  struct MHD_Response *response;


  response = MHD_create_response_from_data (strlen (page), (void *) page, MHD_NO, MHD_NO);
  if (!response)
    return MHD_NO;

		printf("Queueing %s\n", page);
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);

  return ret;
}



int process_get_request (struct MHD_Connection *connection, const char *url, void *coninfo_cls) {

	//int item_group, item_id = 0;
	//char item_name, item_request[32];
	struct connection_info_struct *con_info = (struct connection_info_struct *) coninfo_cls;

	//if (sscanf(url, "/%s/%d/%d/%s", &item_name, &item_group, &item_id, &item_request) == 4) {
	//	printf("Processing HTTP GET Request: Item: %s, Group: %d, ID: %d, Command: %s\n", item_name, item_group, item_id, item_request);
	//}
	char *data = send_serial_cmd(connection, url);
	int size = 1;
	if ((size > 0) && (size <= MAXNAMESIZE)) {
		char *answerstring;
		answerstring = (char*) malloc (MAXANSWERSIZE);
		if (!answerstring) return MHD_NO;

		snprintf (answerstring, MAXANSWERSIZE, answerpage, data);
		con_info->answerstring = answerstring;
		printf("Returning %s\n", con_info->answerstring);
		//free(answerstring);
		return MHD_NO;
	}
	return MHD_YES;
}


int
iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size)
{
  //struct connection_info_struct *con_info = (struct connection_info_struct *) coninfo_cls;


  if ((0 == strncmp (key, "ON", 3)) || (0 == strncmp (key, "OFF", 3))) {

// print command to serial port and return!

/*
      if ((size > 0) && (size <= MAXNAMESIZE))
        {
          char *answerstring;
          answerstring = (char*) malloc (MAXANSWERSIZE);
          if (!answerstring)
            return MHD_NO;

          snprintf (answerstring, MAXANSWERSIZE, answerpage, data);
          con_info->answerstring = answerstring;
        }
      else
        con_info->answerstring = NULL;

*/
      return MHD_NO;
    }

  return MHD_YES;
}

void
request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe)
{
  struct connection_info_struct *con_info =
    (struct connection_info_struct *) *con_cls;


  if (NULL == con_info)
    return;

  if (con_info->connectiontype == POST)
    {
      MHD_destroy_post_processor (con_info->postprocessor);
      if (con_info->answerstring)
        free (con_info->answerstring);
    }

  free (con_info);
  *con_cls = NULL;
}


int answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls) {

	if (NULL == *con_cls) {
		printf("answer_to_connection(): *con_cls == NULL\n");
		struct connection_info_struct *con_info;

		con_info = (connection_info_struct*) malloc (sizeof (struct connection_info_struct));

		if (NULL == con_info) return MHD_NO;

		con_info->answerstring = NULL;

		if (0 == strncmp (method, "POST", 4)) {
			con_info->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void *) con_info);

			if (NULL == con_info->postprocessor) {
				free (con_info);
				return MHD_NO;
			}

>>>>>>> c3b7a1e7f58601fb4559db8673dcf4719e5c6b2e
			con_info->connectiontype = POST;
		} else {
			con_info->connectiontype = GET;
		}

		*con_cls = (void *) con_info;

		return MHD_YES;
	}

	if (0 == strncmp (method, "GET", 4)) {
		printf("answer_to_connection(): method == GET, url = %s\n", url);
		struct connection_info_struct *con_info = (connection_info_struct*) *con_cls;

		if (NULL == con_info->answerstring) {
			printf("answer_to_connection(): con_info->answerstring == NULL\n");
			process_get_request(connection, url, con_info);
			return MHD_YES;
		} else if (NULL != con_info->answerstring) {
			printf("answer_to_connection(): sending answerstring: %s\n", con_info->answerstring);
			return send_page (connection, con_info->answerstring);
		}
	}

	if (0 == strncmp (method, "POST", 4)) {
		printf("answer_to_connection(): method == POST\n");
		struct connection_info_struct *con_info = (connection_info_struct*) *con_cls;

		if (*upload_data_size != 0) {
			MHD_post_process (con_info->postprocessor, upload_data,
			*upload_data_size);
			*upload_data_size = 0;

			return MHD_YES;
		} else if (NULL != con_info->answerstring) {
			return send_page (connection, con_info->answerstring);
		}
	}

	return send_page (connection, errorpage);
}


int powermeter(char *buffer) {
	//pthread_t tid[NUM_INSTALLED_COUNTERS];
	//int t_error;
	curl_args_t curl_args[NUM_INSTALLED_COUNTERS];
	int counter[NUM_COUNTERS];
	uint32_t duration[NUM_COUNTERS];
	int rf12_node_id;
	int rf12_seq;

	if (sscanf(buffer, "Node ID: %d, seq: %d, data = %d %d %d %d %d %d %d %d %d %d", &rf12_node_id, &rf12_seq, &counter[0], &duration[0], &counter[1], &duration[1], &counter[2], &duration[2], &counter[3], &duration[3], &counter[4], &duration[4]) == 12) {

		for (int i = 0 ; i < NUM_INSTALLED_COUNTERS ; i++) {
			memset(&curl_args[i], 0, sizeof curl_args[i]);
			strcpy(curl_args[i].method, "GET");
			//memset(curl_args[i].url, 0, sizeof curl_args[i].url);
			sprintf(curl_args[i].url, counter_urls[i], counter[i]);
			/*
			t_error = pthread_create(&tid[i], NULL, curl_rest, &curl_args[i]);
			if (0 != t_error) {
				fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, t_error);
			} else {
				pthread_detach(tid[i]);
			}
			*/
			curl_rest(&curl_args[i]);
		}
	}
	return(0);
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


void *read_serial(void *null) {
	int res;
	char buf[256];
	//int rcvd_msgid;
	int answer_idx = 0;

 while (1) {
		res = read(serial_fd,buf,255); 
		buf[res]=0;             /* set end of string, so we can printf */
		if ((buf[res-1] == '\n') || (buf[res-1] == '\r')) {
			printf("read_serial(): %s: %s", timestamp(), buf);

			// if there is a msgid put it in the answer array
			if (0 == strncmp(buf, "MSG ID:", 7)) {
				if (answer_idx >= MAX_ANSWERSTRINGS -1) answer_idx = 0;
				printf("read_serial(): Caching serial string in element %d: %s\n", answer_idx, buf);
				memset(&answer_strings[answer_idx], 0, MAX_ANSWERSTRING_LENGTH);
				sprintf(answer_strings[answer_idx++], "%s", buf);
			} else if (0 == strncmp(buf, "REST", 4)) {
				curl_helper(buf);
			}


// This has to go away! ////////////////////////////////////////
/*
*/
			int rf12_node_id;
			if (sscanf(buf, "Node ID: %d", &rf12_node_id) == 1) {
				if (rf12_node_id == 5) {
					powermeter(buf);
				}
			}
/////////////////////////////////////////////////////////////////
		}
	}
	return 0;
}


int main () {
	pthread_t tid[1];
	int t_error;
	struct MHD_Daemon *daemon;
	

	// Register signal and signal handler
	signal(SIGINT, signal_callback_handler);

	kill_power_led();
	open_serial_port();

	// start serial listener
	t_error = pthread_create(&tid[0], NULL, read_serial, NULL);
	fprintf(stderr, "Starting serial read thread\n");
	if (0 != t_error) fprintf(stderr, "Couldn't run thread number %d, errno %d\n", 0, t_error);


	// start webserver
  daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                             &answer_to_connection, NULL,
                             MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                             NULL, MHD_OPTION_END);
  if (NULL == daemon)
    return 1;

  getchar ();

  MHD_stop_daemon (daemon);

  return 0;
}

