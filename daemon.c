#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libconfig.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>

#define EXIT_FAIL -1
#define EXIT_SUCCESS 0

#define RUN_DIR "/tmp/"
#define CFG_PATH "/etc/daemon.conf"

const char* dir_path;
int check_period;

bool read_cfg(const char* cfg_path, const char **dir_path_buf, int *check_period_buf)
{
	config_t cfg;
	config_init(&cfg);
	if (!config_read_file(&cfg, cfg_path))
	{
		syslog(LOG_ERR, "Failed reading configuration file %s", cfg_path);
		config_destroy(&cfg);
		return false;
	}

	const config_setting_t *root = config_root_setting(&cfg);

	const config_setting_t *dir = config_setting_get_member(root, "dir");

	if (dir == NULL)
	{
		syslog(LOG_ERR, "Failed reading dir value from configuration file %s", cfg_path);
		return false;
	}

	DIR* d = opendir(config_setting_get_string(dir));
	if (!d)
	{
		syslog(LOG_ERR, "Failed to access directory from configuration file %s", cfg_path);
		return false;
	}
	closedir(d);


	const config_setting_t *period = config_setting_get_member(root, "period");

	if (period == NULL)
	{
		syslog(LOG_ERR, "Failed reading period value from configuration file %s", cfg_path);
		return false;
	}

	*dir_path_buf = config_setting_get_string(dir);
	*check_period_buf = config_setting_get_int(period);

	return true;
}

void signal_handler(int sig)
{
	switch(sig)
	{
		case SIGHUP:
			if (read_cfg(CFG_PATH, &dir_path, &check_period))
				syslog(LOG_INFO, "Successfully redirected daemon to %s", dir_path);
			break;
		case SIGTERM:
			syslog(LOG_INFO, "Successfully stopped daemon");
			exit(EXIT_SUCCESS);
			break;
	}
}

void daemonize()
{
	pid_t pid = fork();

	if (pid < 0)
	{
		syslog(LOG_ERR, "Failed forking from parent process");
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
		syslog(LOG_ERR, "Failed to set SID");
		exit(EXIT_FAIL);
	}

	if (chdir(RUN_DIR) < 0)
	{
		syslog(LOG_ERR, "Failed to move running directory");
		exit(EXIT_FAIL);
	}

	for (int n_desc = sysconf(_SC_OPEN_MAX); n_desc >= 0; n_desc--)
	{
		close(n_desc);
	}
}

void check_if_modified(const char* path, int period)
{
	struct stat file_info;
	if (stat(path, &file_info) < 0)
	{
		//syslog(LOG_ERR, "%s %s", "Failed accessing file", file_path);
		return;
	}
	if (S_ISREG(file_info.st_mode))
	{
		int cur_time = (int)time(NULL);
		int mod_time = (int)file_info.st_mtime;

		if (cur_time - period < mod_time)
		{
			char *str_mod_time = (char*)malloc(20 * sizeof(char));
			strftime(str_mod_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&file_info.st_ctime));

			syslog(LOG_INFO, "%s %s %s", path, "modified at", str_mod_time);

			free(str_mod_time);
			return;
		}
	}
	else if (S_ISDIR(file_info.st_mode))
	{
		DIR *dir;
		struct dirent *entry;

		if((dir = opendir(path)) == NULL)
		{
			//syslog(LOG_ERR, "%s %s", "Failed opening directory", file_path);
			return;
		}

		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_name[0] != '.')
			{
				int new_path_len = strlen(path) + strlen(entry->d_name) + 2;
				char* new_path = (char*)malloc(new_path_len * sizeof(char*));

				snprintf(new_path, new_path_len, "%s/%s", path, entry->d_name);

				check_if_modified(new_path, period);

				free(new_path);
			}
		}
		closedir(dir);
	}
}

void main()
{
	openlog("daemon", LOG_PID, LOG_USER);

	if (!read_cfg(CFG_PATH, &dir_path, &check_period))
		exit(EXIT_FAIL);

	daemonize();

	syslog(LOG_INFO, "Successfully started daemon at %s", dir_path);

	while(true)
	{
		sleep(check_period);
		check_if_modified(dir_path, check_period);
	}
}
