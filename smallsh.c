#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE_LEN 2048
#define MAX_NUM_ARGS 512
#define GENERIC_STRING_SIZE 256

void parse_input(char * raw_input, char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int * num_args) {
	const char delim[3] = " \n";
	char* token;
	(*num_args) = 0;
	token = strtok(raw_input, delim);
	while (token != NULL) {
		strcpy((args)[*num_args], token);
		token = strtok(NULL, delim);
		(*num_args) += 1;
	}
	return;
}

void exec_program(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int num_args) {
	execvp(args[0], args);	
	perror("Failed to execute command");
	exit(2);
}

int wait_for_child(pid_t spawnpid) {
	int child_exit_code;
	spawnpid = waitpid(spawnpid, &child_exit_code, 0);
	return WEXITSTATUS(child_exit_code);
}

int new_process(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int num_args) {
	pid_t spawnpid = -5;
	int result;
	spawnpid = fork();
	switch(spawnpid) {
		case -1:
			return 1;
		case 0:
			exec_program(args, num_args);
			break;
		default:
			result = wait_for_child(spawnpid);
	}
	return result;
}
int process_command(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int num_args, int last_status) {
	int result;
	if (!strcmp(args[0], "cd")) {
		if (num_args <= 2) {
			result = chdir(args[1]);
			if (result == -1) {
				printf("%s\n", strerror(errno));
				fflush(stdout);
				return 1;
			} 
			return 0;
		} else {
			perror("Too many arguments to function\n");
			return 1;
		}
	} else if (!strcmp(args[0], "status")) {
		if (num_args == 1) {
			printf("exit value %d\n", last_status);
			fflush(stdout);
			return 0;
		} else {
			perror("Too many arguments to function\n");
			fflush(stdout);
			return 1;
		}
	} else {
		return new_process(args, num_args);
	}
}

int run_loop() {
	char *input = NULL, args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], command[MAX_LINE_LEN];
	int i, num_args, chars_entered, last_status;
	size_t line_size = 0;
	while(1) {
		memset(command, '\0', MAX_LINE_LEN * sizeof(char));
		for (i = 0; i < MAX_NUM_ARGS; i++) {
			memset(args[i], '\0', GENERIC_STRING_SIZE * sizeof(char));
		}
		while(1) {
			printf(": ");
			fflush(stdout);
			chars_entered = getline(&input, &line_size, stdin);
			if (chars_entered == -1) {
				clearerr(stdin);
			} else {
				break;
			}
		}
		parse_input(input, args, &num_args);
		last_status = process_command(args, num_args, last_status);
		if (!strcmp(args[0], "exit")) {
			free(input);
			return 0;
		}
			
	}

}
int main() {
	return run_loop();
}
