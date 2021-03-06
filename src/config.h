#define PORT            8888
#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512

#define MAX_ANSWERSTRINGS 10
#define MAX_ANSWERSTRING_LENGTH 256

#define SERIAL_PORT "/dev/ttyS1"
#define BAUDRATE B57600

#define POWER_LED_FILE "/proc/diag/led/power"

#define NUM_COUNTERS 5
#define NUM_INSTALLED_COUNTERS 3


const char counter_urls[NUM_INSTALLED_COUNTERS][256] = {
	"http://yavdr:81/middleware.php/data/46b27ba0-6ae1-11e1-91d0-4315961506a9.json?operation=add&value=%d",
	"http://yavdr:81/middleware.php/data/60435230-6ae1-11e1-bc8b-7f8bbcdebe7a.json?operation=add&value=%d",
	"http://yavdr:81/middleware.php/data/65c072e0-6ae1-11e1-879a-7bc14050b5a0.json?operation=add&value=%d",
};
