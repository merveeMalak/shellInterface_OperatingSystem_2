#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

//global variables
char input_line[100];
char* parsed[10]; 
int token_counter; //holds number of argv
int builtin_commands_number = 4; //dir, cd, history, bye
struct history_node* hist_queue;  //holds history_queue
int history_counter = 1;

//keeps commands to be put into history queue as nodes
struct history_node{
	char* command_name;
	struct history_node* next_node;
};

/*Divides the input from the command line according to white
spaces and stores it in parsed array.
*/
void parse_line(){
	for (int i = 0; i <10; i++){ //each element in the array is initialized to NULL 
		parsed[i] = NULL;
	}
	token_counter = 0;
	char *token = strtok(input_line, " "); //splits into tokens
	while (token != NULL && token_counter < 10){ //token must not be NULL and token counter must be less than 10
		parsed[token_counter] = token;
		token = strtok(NULL, " ");
		token_counter++;
	}
}
//prints the current working directory
void dir(){
	char path_buffer[100];
	if (getcwd(path_buffer, sizeof(path_buffer)) == NULL){
		perror("getcwd() error!");
	}else{
		printf("%s\n", path_buffer);
	}
	
}
//if the desired directory is available, change the current directory to <directory>
void cd(){
	char *dir = parsed[1];
	if (dir == NULL || strcmp(dir, "")== 0){ //the directory argument is not present
		dir = getenv("HOME"); //set directory HOME
	}
	char path[100]; //holds current directory
	if (getcwd(path, sizeof(path)) == NULL){
			perror("getcwd() error!");
	}else{
		char relative_path[100];
		if (chdir(dir) != 0){ //directory is not absolute path
			strcpy(relative_path, path);
			strcat(relative_path, "/");
			strcat(relative_path, dir);
			if (chdir(relative_path) != 0){ //directory is not relative path
				perror("chdir() error!");
			}else{
				setenv("PWD", relative_path,1);
			}
		}else{
			setenv("PWD", dir, 1);
		}
	}
}
//prints 10 most recently entered commands	
void history(){
	int index = 1;
	struct history_node* temp_hist_queue = hist_queue; 
	int counter = history_counter;
	while (counter > 1){
		printf("[%d] %s\n", index, temp_hist_queue->command_name);
		counter--;
		index++;
		temp_hist_queue=temp_hist_queue->next_node;
	}
}
//creates the command as a node
struct history_node* create_hist_node(char *command){
	struct history_node* new_node = malloc(sizeof(hist_queue));
	new_node->command_name = malloc(sizeof(strlen(command) + 1));
	strcpy(new_node->command_name,command);
	new_node->next_node = NULL;
	return new_node;

}
//adds the command to the history_queue
void add_history(char *command){
	if (history_counter == 1){ //history_queue is empty
		hist_queue = create_hist_node(command);
		history_counter++;
	}else if (history_counter < 11){ //history queue is not full
		struct history_node* temp_node = hist_queue;
		while(temp_node->next_node != NULL){ //goes to last node in history_queue
			temp_node = temp_node->next_node;
		}
		temp_node->next_node = create_hist_node(command); //adds to the end of the history_queue
		history_counter++;
	}else{  //history_queue is full
		hist_queue = hist_queue->next_node; //first node is deleted from queue
		struct history_node* temp_node = hist_queue;
		while(temp_node->next_node != NULL){ //goes to last node in history_queue
			temp_node = temp_node->next_node;
		}
		temp_node->next_node = create_hist_node(command); //adds to the end of the history_queue
	}
}
//exit program
void bye(){
	exit(0);
}

//checks whether the pipe operation will be performed
int check_pipe(){
	for (int i = 0; i < token_counter; i++){
		if (strcmp(parsed[i],"|")== 0){ //there is "|" return index  
			return i;
		}
	}
	return -1; //else returns -1
}

//execute the pipe, flag_pipe is index of "|
void progress_pipe(int flag_pipe){
	char* arg1[10]; //holds the first command 
	char* arg2[10]; //holds the second command
	for (int i = 0; i < 10; i++){ //each element in the arrays is initialized to NULL 
		arg1[i] = NULL;
		arg2[i] = NULL;
	}
	int j = 0;
	for (int i = 0; i < flag_pipe; i++){ //first command
		arg1[i] = parsed[i];
	}
	for (int i = flag_pipe+1; i <token_counter;i++){ //second command
		arg2[j] = parsed[i];
		j++;
	}
	int fd[2];
	if (pipe(fd) < 0){ //pipe is not created
		perror("There is an error pipe creation!");
	}else{
		pid_t pid = fork();
		if (pid<0){ 
			perror("Error fork()!");
		}
		else if (pid == 0){ //child process
			dup2(fd[1],1); //duplicates fd[1] to stdout
			close(fd[0]);  //close fd[0]
			close(fd[1]);  //close fd[1]
			execvp(arg1[0], arg1); //execute first command
			perror("Error execvp()!"); //first command is invalid
		}else{ //parent process
			dup2(fd[0],0); //duplicates fd[0] to stdin
			close(fd[0]); //close fd[0]
			close(fd[1]); //close fd[1]
			execvp(arg2[0], arg2); //execute second command
			perror("Error execvp()!"); //second command is invalid
		}
	}
}
//there is not pipe operation and command is not builtin 
void progress_without_pipe(){
	pid_t pid;
	pid = fork();
	if (pid < 0){ 
		perror("Fork failed");
	}else if (pid == 0){ //child process
		if (parsed[token_counter-1] == "&"){ //program should not wait for the task complete
			parsed[token_counter-1] = NULL;
		}
		if (execvp(parsed[0], parsed) == -1){ //execute progress
			perror("Invalid command!"); //invalid command
			exit(0);	
		}
	}else if(parsed[token_counter-1] != "&"){ //program should wait for the task complete
		wait(NULL);
	}
}

//checks if the command is a builtin command 
bool is_builtin(char* command){
	if (strcmp(parsed[0], "dir") == 0){
			return true;
		}else if (strcmp(parsed[0], "cd") == 0){
			return true;
		}else if (strcmp(parsed[0], "history") == 0){
			return true;
		}else if (strcmp(parsed[0], "bye") == 0){
			return true;
		}
	return false;
}

//execute builtin command
void execute_builtin(char* command){
	if (strcmp(parsed[0], "dir") == 0){
		dir();
	}else if (strcmp(parsed[0], "cd") == 0){
		cd();
	}else if (strcmp(parsed[0], "history") == 0){
		history();
	}else if (strcmp(parsed[0], "bye") == 0){
		bye();
	}
}
int main(){
	while(1){
		printf("myshell>");
		fgets(input_line, 100, stdin); //gets the input
		input_line[strcspn(input_line, "\n")] = 0;
		if (input_line[0] == '\0' || input_line[0] == '\n'){ //if input is " " or "\n" 
			continue;
		}
		add_history(input_line); //adds the command to history_queue
		parse_line(); //parses input
		if (is_builtin(parsed[0])){// builtin command
			execute_builtin(parsed[0]);
		}else{
			int flag_pipe = check_pipe();
			if (flag_pipe != -1){ //there is pipe operation
				progress_pipe(flag_pipe);
			}else{ //command is not builtin or pipe operation
				progress_without_pipe();
			}
		}			
	}	
}
