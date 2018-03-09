#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE_LEN 2048
#define MAX_NUM_ARGS 512
#define GENERIC_STRING_SIZE 256

#define BG_MODE_ENV_VAR "BG_MODE" // 1 == bg allowed, 0 == bg not allowed

void parse_input(char * raw_input, char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int * num_args) {
	const char delim[3] = " \n"; // use space and newline for delim
	char *token, split_start[GENERIC_STRING_SIZE], split_end[GENERIC_STRING_SIZE];
	int i;
	pid_t current_pid;
	(*num_args) = 0;
	token = strtok(raw_input, delim);
	while (token != NULL) { // for each token . . .
		strcpy((args)[*num_args], token);
		token = strtok(NULL, delim); // get the token
		for (i = 0; i < (GENERIC_STRING_SIZE - 1); i++) { // expand $$
			if (args[*num_args][i] == '$' && args[*num_args][i + 1] == '$') {
				current_pid = getpid();
				sprintf(split_start, "%.*s", i, args[*num_args]); // split string into section before and after $$
				sprintf(split_end, "%.*s", GENERIC_STRING_SIZE - (i + 2), &(args[*num_args][i + 2]));
				sprintf(args[*num_args], "%s%d%s",  // combine them
						split_start,
						(int) current_pid, 
						split_end);
			}	
		}
		(*num_args) += 1;
	}
	return;
}

void handle_sigtstp(int signo) {
	char * entermsg = "Entering foreground-only mode (& is now ignored)\n", *exitmsg = "Exiting foreground-only mode\n";
	if (!strcmp(getenv(BG_MODE_ENV_VAR), "1")) { // if not in bg only mode, enter it
		setenv(BG_MODE_ENV_VAR, "0", 1);
		write(1, entermsg, 50);
	} else {
		setenv(BG_MODE_ENV_VAR, "1", 1);	 // if in bg only mode, exit it
		write(1, exitmsg, 30);
	}
}
void define_shell_signal_behavior() {
	// make sure that the shell ignores sigint and handles fg mode switching
	struct sigaction sigint_action, sigtstp_action;

	sigint_action.sa_handler = SIG_IGN;
	sigfillset(&sigint_action.sa_mask);

	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);

	sigtstp_action.sa_handler = handle_sigtstp;
	sigfillset(&sigtstp_action.sa_mask);
	sigtstp_action.sa_flags = 0;

	sigaction(SIGTSTP, &sigtstp_action, NULL); 

}
void define_subprocess_signal_behavior() {
	// make sure that subprocesses don't ignore sigint signals
	sigset_t ignore_signal_set;		
	struct sigaction sigint_action;

	sigemptyset(&ignore_signal_set);
	sigaddset(&ignore_signal_set, SIGINT);
	sigint_action.sa_handler = SIG_DFL;
	sigfillset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;

	sigaction(SIGINT, &sigint_action, NULL);
}

void exec_program(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], char input_file[GENERIC_STRING_SIZE], char output_file[GENERIC_STRING_SIZE], int is_bg, int num_args, pid_t * background_processes) {
	int i, input_fd, output_fd;
	char** exec_args = malloc((num_args + 1) * sizeof(char*));
	define_subprocess_signal_behavior();
	for (i = 0; i < num_args; i++) {
		// need to malloc new heap array to pass to execvp because it won't accept static 2d arrays
		exec_args[i] = malloc(GENERIC_STRING_SIZE * sizeof(char));
		memset(exec_args[i], '\0', GENERIC_STRING_SIZE * sizeof(char));
		strcpy(exec_args[i], args[i]);
	}
	exec_args[num_args] = NULL; // set last entry of exec_args to NULL as terminator for execvp
	if (input_file[0]) { // redirect input if input filename is passed
		input_fd = open(input_file, O_RDONLY);
		if (input_fd == -1) {
			perror("Failed to open input file");
			exit(2);
		} else {
			input_fd = dup2(input_fd, 0);
			if (input_fd == -1) {
				perror("Failed to redirect input");
				exit(2);
			}
		}
	}
	if (output_file[0]) { // redirect output if output filename is passed
		output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (output_fd == -1) {
			perror("Failed to open output file");
			exit(2);
		} else {
			output_fd = dup2(output_fd, 1);
			if (output_fd == -1) {
				perror("Failed to redirect ouput");
				exit(2);
			}
		}
	}
	execvp(args[0], exec_args);	// transfer control to new program
	perror("Failed to execute command"); // if our program still has control, something went wrong
	exit(2);
}

int wait_for_child(pid_t spawnpid, int is_bg, pid_t * background_processes) {
	int child_exit_code;
	if (is_bg && !strcmp(getenv(BG_MODE_ENV_VAR), "1")) { // if bg process can be run, don't wait for process to finish, just print its pid
		printf("background pid is %d\n", (int) spawnpid);
		fflush(stdout);
		return 1;
	}
	spawnpid = waitpid(spawnpid, &child_exit_code, 0); // otherwise, wait for process
	return child_exit_code;
}

int new_process(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], char input_file[GENERIC_STRING_SIZE], char output_file[GENERIC_STRING_SIZE], int is_bg, int num_args, pid_t* background_processes) {
	pid_t spawnpid = -5;
	int result, i;
	spawnpid = fork();
	switch(spawnpid) {
		case -1:
			return 1;
		case 0:
			exec_program(args, input_file, output_file, is_bg, num_args, background_processes); // if new process, execute the program in args
			break;
		default:
			if (is_bg && !strcmp(getenv(BG_MODE_ENV_VAR), "1")) { // if background process, don't wait for process -- just add it to background_processes
				for (i = 0; i < 256; i++) {
					if (!background_processes[i]) {
						background_processes[i] = spawnpid;
						break;
					}
				}
			}
			result = wait_for_child(spawnpid, is_bg, background_processes); // call wait_for_child function 
	}
	return result;
}

// command_args is the args list for the command entered by the user
// input_file is the input file for the process
// output_file is the output file for the process
// is_bg is a flag set if the program will run the in the background
// num_args initially is the number of args in args and is changed to the number of args in command_args
void split_args(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], char command_args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], char input_file[GENERIC_STRING_SIZE], char output_file[GENERIC_STRING_SIZE], int *bg, int * num_args) {
	int i = 1;
	int new_num_args = 0;
	strcpy(command_args[0], args[0]);
	while (i < (*num_args) && args[i][0] != '<' && args[i][0] != '>' && args[i][0] != '&') { // copy args into command_args until finding <, >, or &
		strcpy(command_args[i], args[i]);
		i++;
	}
	new_num_args = i;
	if (i < *num_args && args[i][0] == '<') { // set input file is input char is found
		i++;
		strcpy((input_file), args[i]); 
		i++;
	} 
	if (i < *num_args && args[i][0] == '>') { // set output file if output char is found
		i++;
		strcpy((output_file), args[i]);
		i++;
	}
	if (i < *num_args && args[i][0] == '&') { // set is_bg if ampersand is found
		(*bg) = 1;	
	} else {
		(*bg) = 0;
	}
	*num_args = new_num_args; // set num_args to the number of command args instead of the total number of word tokens entered
}

int process_command(char args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], int num_args, int last_status, pid_t * background_processes) {
	int result, is_bg = 0;
	char command_args[MAX_NUM_ARGS][GENERIC_STRING_SIZE];
	char input_file[GENERIC_STRING_SIZE];
	char output_file[GENERIC_STRING_SIZE];
	// clear all arrays used
	memset(command_args, 0, MAX_NUM_ARGS * sizeof(char*));
	memset(input_file, '\0', GENERIC_STRING_SIZE * sizeof(char));
	memset(output_file, '\0', GENERIC_STRING_SIZE * sizeof(char));
	split_args(args, command_args, input_file, output_file, &is_bg, &num_args);
	if (!args[0][0] || args[0][0] == '#') { // completely ignore comment lines
		return 0;
	} else if (!strcmp(args[0], "cd")) { // process cd operation using chdir() function
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
	} else if (!strcmp(args[0], "status")) { // get status
		if (num_args == 1) {
			if (WIFEXITED(last_status)) { // check if program exited for was killed by a signal
				printf("exit value %d\n", WEXITSTATUS(last_status));
			} else {
				printf("terminated by signal %d\n", WTERMSIG(last_status));
}
			fflush(stdout);
			return 0;
		} else {
			perror("Too many arguments to function\n");
			fflush(stdout);
			return 1;
		}
	} else {
		return new_process(command_args, input_file, output_file, is_bg, num_args, background_processes); // otherwise execute new process
	}
}

int run_loop() {
	char *input = NULL, args[MAX_NUM_ARGS][GENERIC_STRING_SIZE], command[MAX_LINE_LEN];
	int i, num_args, chars_entered, last_status;
	size_t line_size = 1;
	setenv(BG_MODE_ENV_VAR, "1", 1);
	define_shell_signal_behavior();
	pid_t background_processes[256], exit_pid;
	for (i = 0; i < 256; i++) { // initialize all background_processes in array as zero
		background_processes[i] = 0;
	}
	while(1) { // main loop
		for (i = 0; i < 256; i++) {
			if (background_processes[i]) { // check each bg process and try waiting for it
				exit_pid = 0;
				exit_pid = waitpid(background_processes[i], &last_status, WNOHANG);
				if (exit_pid > 0) {
					if (WIFEXITED(last_status)) {
						printf("Process %d termined with status %d\n", (int) exit_pid, WEXITSTATUS(last_status)); // alert the user if the process has exited or used a signal
					} else {
						printf("Process %d was killed by signal %d\n", (int) exit_pid, WTERMSIG(last_status));
					}
					background_processes[i] = 0; // reset background process
				}
			}	
		}
		memset(command, '\0', MAX_LINE_LEN * sizeof(char)); // reset command and args arrays
		for (i = 0; i < MAX_NUM_ARGS; i++) {
			memset(args[i], '\0', GENERIC_STRING_SIZE * sizeof(char));
		}
		while(1) {
			printf(": ");
			fflush(stdout);
			chars_entered = getline(&input, &line_size, stdin); // get user input
			if (chars_entered == -1) {
				clearerr(stdin);
			} else {
				break;
			}
		}
		parse_input(input, args, &num_args); // parse input
		if (!strcmp(args[0], "exit")) {
			free(input); // clear input mem and exit program
			return 0;
		}
		last_status = process_command(args, num_args, last_status, background_processes);
	}

}
int main() {
	// do everything in run_loop()
	return run_loop();
}
