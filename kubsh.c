#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_FILE ".kubsh_history"
sig_atomic_t signal_received = 0;

void echo(char *line) {
	printf("%s\n", line);
}

void sig_handler(int signum) {
	signal_received = signum;
	printf("Configuration reloaded");
}

void disk_info(char* device) {
	printf("Disk information for %s: \n", device);

	char command[256];
	snprintf(command, sizeof(command), "sudo fdisk -l %s 2>/dev/null", device);

	int result = system(command);

	if (result != 0) {
		printf("Error: Can not get disk information for %s\n", device);
		printf("Try running with sudo or check device name\n");
	}
}

void print_env(char* var_name) {
	char* value = getenv(var_name);
	if (value == NULL) {
		printf("Env variable %s not found\n", var_name);
		return;
	}
	
	printf("Env variable %s\n", var_name);

	char* copy = strdup(value);
	if (copy == NULL) {
		printf("Memory allocation error\n");
		return;
	}

	char* token = strtok(copy, ":");
	int count = 1;
	while (token != NULL) {
		printf("%d: %s\n", count++, token);
		token = strtok(NULL, ":");
	}

	free(copy);
}

int main() {
    signal(SIGHUP, sig_handler);
    read_history(HISTORY_FILE);
    
    char *input;

    while (1) {
	input = readline("$ ");

	if (signal_received) {
	    signal_received = 0;
      	    continue;
	}

	if (input == NULL) {
		printf("\nExit (Ctrl+D)\n");
		break;
	}

	add_history(input);


	if (strlen(input) == 0) {
		free(input);
		continue;
	}
        if (strcmp(input, "\\q") == 0) {
            printf("Exit\n");
            break;
        } else if (strncmp(input, "echo ", 5) == 0) {
	    echo(&input[5]);
	} else if (!strncmp(input, "\\l /dev/sda", 12)) {
	       disk_info("/dev/sda");	
	} else if (!strncmp(input, "\\e $", 4))  {
		char* var_name = input + 4;
		if (*var_name != '\0') {
			print_env(var_name);
		} else {
			printf("Usage: \\e $VARIABLE_NAME\n");
		}	
	} else {
		int ret = system(input);
		if (ret == -1) {
			printf("%s: command not found\n", input);
		}
	}

	free(input);
    }
    write_history(HISTORY_FILE);
    return 0;
}
