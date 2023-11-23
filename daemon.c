#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libconfig.h>
#include <time.h>
#include <stdbool.h>

#define EXIT_FAIL -1
#define EXIT_SUCCESS 0

#define RUN_DIR "/tmp/"
#define CFG_PATH "/home/azazuent/code/daemon/daemon.cfg"
#define LOG_PATH "/home/azazuent/code/daemon/daemon.log"

#define INFO 0
#define ERROR -1
#define DEBUG 1

bool hup_received = false;
bool term_received = false;

struct cfg
{
	int n_dir;
	const char* *dirs;

	int r_period;
};

bool log_write(const char* log_path, const char* log_message, int status)
{
	FILE* log_file = fopen(log_path, "at");
	if (!log_file)
		return false;

	time_t cur_time = time(NULL);
	char log_time[20];
	strftime(log_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&cur_time));

	const char* log_status;
	switch (status)
	{
		case INFO:
			log_status = "[INFO]";
			break;
		case ERROR:
			log_status = "[ERROR]";
			break;
		case DEBUG:
			log_status = "[DEBUG]";
			break;
		default:
			log_status = "[UNDEFINED]";
			break;
	}
	fprintf(log_file, "%s %s: %s\n", log_time, log_status, log_message);

	fclose(log_file);
	return true;
}

/*
struct cfg* read_cfg(const char* cfg_path)
{
	config_t cfg;
	config_init(&cfg);
	if (!config_read_file(&cfg, cfg_path))
	{
		log_write(LOG_PATH, "Failed reading configuration file", ERROR);
		config_destroy(&cfg);
		return NULL;
	}
}
*/
void signal_handler(int sig)
{
	switch(sig) 
	{
		case SIGHUP:
			hup_received = true;
			break;
		case SIGTERM:
			term_received = true;
			break;
	}
}

void daemonize()
{
	pid_t pid = fork();

	if (pid < 0)
	{
		log_write(LOG_PATH, "Failed forking from parent process", ERROR);
		exit(EXIT_FAIL);
	}

	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);

	umask(0);

	pid_t sid = setsid();

	if (sid < 0)
	{
		log_write(LOG_PATH, "Failed to set SID", ERROR);
		exit(EXIT_FAIL);
	}

	if (chdir(RUN_DIR) < 0)
	{
		log_write(LOG_PATH, "Failed to move running directory", ERROR);
		exit(EXIT_FAIL);
	}

	for (int n_desc = sysconf(_SC_OPEN_MAX); n_desc >= 0; n_desc--)
	{
		close(n_desc);
	}
}

int main()
{
	daemonize();

	log_write(LOG_PATH, "Successfully started daemon", INFO);

	int i = 0;
	while(true)
	{
		sleep(1);
		i++;
		if (term_received) break;
	}

	log_write(LOG_PATH, "Successfully stopped daemon", INFO);
	return 0;
}
