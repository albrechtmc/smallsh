/*
CS344 Introduction to Operating Systems
Small Shell Created by Matthew Albrecht
August 2, 2019
This is a small shell that runs on top of the bash shell.  Key words 
include status, cd, and cd (path).  The # symbol in the front of a 
line wil denote a comment, and the whole line will be ignored.  A $$
symbol will be expanded to teh pid of the current running process, and 
an & at the end of a line will run the process in the background.  The 
signals registered with teh program include SIGINT, which will kill any
current funning background processes, and SIGTSTP, which will block
any commands from running in the background. "exit" will end the program.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <locale.h>
#include <langinfo.h>
#include <stdint.h>
#include <signal.h>

//constants
#define ROWS 512
#define COLUMNS 50
#define PROCESSES 30

//variables to signal whether the shell is in foreground only mode
int foreground = 0;
int sigtstpFlag = 0;
/*******************************************************************
This is the function that gets input from the user.  It takes the 
array that stores the user's input.  Every space breaks up the input
and stores in in the next row as a separite word.  512 words can be
stored, with each word up to 50 characters.
*******************************************************************/
void getUserInput(char userInput[ROWS][COLUMNS])
{
	int i, j;
	char c;
	//print the prompt
	printf(":");
	fflush(stdout);
	i = 0;
	j = 0;
	c = ' ';
	//continue this loop until newline or 512 words is read
	while (c != '\n' && i < ROWS)
	{
		c = getchar();
		//store each character in the array
		while (c != ' ' && c != '\n')
		{
			userInput[i][j] = c;
			j++;
			c = getchar();
		}
		j = 0;
		i++;
	}
}
/*******************************************************************
This function expands $$ to the current Process ID.  It takes the
array that stores the user's input.  Each time it finds an instance
of $$ it inserts the PID.
*******************************************************************/
//perform variable expansion on $$
int checkPID(char userInput[ROWS][COLUMNS])
{
	int i = 0, j, m;
	int pid = getpid();//the current PID
	char charPID[10] = { '\0' };//store the PID as a character
	char temp[COLUMNS] = { '\0' };//temporary string
	//read until the last word
	while (i < ROWS && userInput[i][0] != '\0')
	{
		j = 0;
		m = 0;
		//read the word until the null terminator
		while (userInput[i][j] != '\0')
		{
			//look for $$
			if (userInput[i][j] == '$')
				if (userInput[i][j+1] == '$')
					{
						//convert the PID to a character string
						sprintf(charPID, "%d", pid);
						//convert the word with $$ to a temporary string
						for (m = 0; m < j; m++)
						{
							temp[m] = userInput[i][m];
						}
						//cat the PID to the string
						strcat(temp, charPID);
						m = m + strlen(charPID);
						int n = j;
						//convert the remainder of the word
						while (userInput[i][n+2] != '\0')
						{
							temp[m] = userInput[i][n+2];
							n++;
							m++;
						}
						//store the new word with the PID to the array
						strcpy(userInput[i], temp);
						j = n;
					}
			j++;
		}
		i++;
		//reset the temp array
		memset(temp, '\0', sizeof(temp[0]) * COLUMNS);
	}
	return pid;
}
/*******************************************************************
This is the function that looks for a comment.  It takes the
array that stores the user's input.  If the first spot is a # symbol, 
1 is returned.  If no comment present return 0.
*******************************************************************/
int checkComment(char userInput[ROWS][COLUMNS])
{
	if (userInput[0][0] == '#')
		return 1;
	else
		return 0;
}
/*******************************************************************
This is the function that adjusts the direction of stdin and sdtout.  
It takes the array that stores the user's input, and an array that 
stores the 3 positions of stdin, stdout, and stderr.  The function looks
for the < or > symbol and stores the redirection in the diraction
array.
*******************************************************************/
int redirection(char userInput[ROWS][COLUMNS], int direction[3])
{
	int i = 0, j, m;
	//scan through array until all words are read
	while (i < ROWS && userInput[i][0] != '\0')
	{
		//if output redirection
		if (userInput[i][0] == '<' && userInput[i][1] == '\0')
		{
			direction[0] = open(userInput[i + 1], O_RDONLY);
			//run this loop twice to remove the < symbol and the location
			for (m = 0; m < 2; m++)
			{
				j = i;
				while (userInput[j][0] != '\0')
				{
					strcpy(userInput[j], userInput[j + 1]);
					j++;
				}
			}
		}
		//if input redirection
		if (userInput[i][0] == '>' && userInput[i][1] == '\0')
		{
			direction[1] = open(userInput[i + 1], O_WRONLY | O_CREAT, 0644);
			//run this loop twice to remove the > symbol and the location
			for (m = 0; m < 2; m++)
			{
				j = i;
				while (userInput[j][0] != '\0')
				{
					strcpy(userInput[j], userInput[j + 1]);
					j++;
				}
			}
		}
		i++;
	}
	//if error with redirection
	if (direction[0] == -1 || direction[1] == -1)
		return 1;
	else
		return 0;
}
/*******************************************************************
This is the function that looks for the & symbol in the last spot.
It takes the array that stores the user's input.  If an ampersand is
present, the return value is 1, if it is not, return 0.
*******************************************************************/
int checkBackground(char userInput[ROWS][COLUMNS])
{
	int i = 0;
	while (i < ROWS && userInput[i][0] != '\0')
	{
		if (userInput[i][0] == '&' && userInput[i][1] == '\0')
			if (userInput[i + 1][0] == '\0')
			{
				userInput[i][0] = '\0';
				return 1;
			}
		i++;
	}
	return 0;
}
/*******************************************************************
This function executes if the SIGINT signal is sent.  It simply
ends the process.
*******************************************************************/
void catchSIGINT(int signo)
{
	exit(0);
}
/*******************************************************************
This function executes if SIGTSTP is sent.  It adjusts the foreground
variable to ignore the & symbol.
*******************************************************************/
void catchSIGTSTP(int signo)
{
	//if not foreground, enter foreground mode
	if (foreground == 0)
	{
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		fflush(stdout);
		foreground = 1;
		sigtstpFlag = 1;
	}
	else//exit foreround mode
	{
		char* message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		fflush(stdout);
		foreground = 0;
		sigtstpFlag = 1;
	}
}
//main function
int main()
{
	//structures to handle signals
	struct sigaction sigint_action = { 0 }, sigtstp_action = { 0 };
	sigint_action.sa_handler = SIG_IGN;
	sigfillset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;

	sigtstp_action.sa_handler = catchSIGTSTP;
	sigfillset(&sigtstp_action.sa_mask);
	sigtstp_action.sa_flags = 0;

	pid_t spawnpid = -5;
	int childExitStatus = -5;//exit status of foreground child process
	pid_t runningPID[PROCESSES] = { 0 };//process ID of all running processes
	pid_t childAlive[PROCESSES] = { 1 };//stutus (running or exited) of all running processes (corresponds to runningPID)
	pid_t actualPid;
	runningPID[30] = 0;

	int i, j, m;//counters
	int pid, comment;
	//exit statuses and termination signals, initially set to impossible values
	int exitStatus = -5;
	int termSignal = -5;
	int backgroundExitStatus = -5;
	int backgroundTermSignal = -5;
	pid = getpid();//current process PID

	//the array that will hold the user input.  Each word is held in a separate row,
	//and each word can have "COLUMNS" characters in it.  Empty rows begin with '\0'
	char userInput[ROWS][COLUMNS];
	//pointer to the input array, for use in the exec function
	char* args[ROWS];
	//array to hold all of the exit statuses of the running processes
	int childExitStatuses[PROCESSES] = { 0 };
	//redirection array
	int direction[3] = { 0, 1, 2 };
	//perinant stdout and stdin integers
	int saved_stdout = dup(1);
	int saved_stdin = dup(0);
	//path to dev/null
	int devNull = open("/dev/null", O_RDWR);
	int background = 0;//background flag
	
	memset(userInput, '\0', sizeof(userInput[0][0]) * ROWS * COLUMNS);
	//reference: https://stackoverflow.com/questions/2910377/get-home-directory-in-linux
	//variables to store the home directory of the user
	struct passwd *pw = getpwuid(getuid());//need getpwuid_r if multithreaded
	const char *homedir = pw->pw_dir;
	char cwd[4096];//string holding the current working directory
	memset(cwd, '\0', sizeof(cwd[0]) * 4096);
	fflush(stdout);
	//sigaction functions
	sigaction(SIGINT, &sigint_action, NULL);
	sigaction(SIGTSTP, &sigtstp_action, NULL);
	//run this loop until exit is received
	while (1)
	{
		for (j = 0; j < ROWS; j++)
			args[j] = NULL;
		//check the background processes and display message to user if any were ended or terminated by signal
		if (backgroundExitStatus != -5)
		{
			printf("Background PID %d is done: exit value %d\n", runningPID[m], backgroundExitStatus);
			fflush(stdout);
			runningPID[m] = 0;
			childAlive[m] = 1;
		}
		if (backgroundTermSignal != -5)
		{
			printf("Background PID %d is done: terminated by signal %d\n", runningPID[m], backgroundTermSignal);
			fflush(stdout);
			runningPID[m] = 0;
			childAlive[m] = 1;
		}
		//reset the array with process IDs.  If a process has stopped, 
		//move all of the running process IDs up to fill the empty spot, 
		//and adjust the corresponding runningPID array to match.
		for (i = 0; i < PROCESSES; i++)
		{
			if (runningPID[i] == 0 && runningPID[i+1] != 0)
			{
				runningPID[i] = runningPID[i + 1];
				childExitStatuses[i] = childExitStatuses[i + 1];
				runningPID[i + 1] = 0;
				childExitStatuses[i + 1] = 0;
			}
		}
		if (sigtstpFlag == 1)
		{

		}
		//get the user input and display the prompt
		getUserInput(userInput);
		//get the PID
		pid = checkPID(userInput);
		//check if user input is a comment
		comment = checkComment(userInput);
		//check the user input if run in background
		background = checkBackground(userInput);

		//check if the user typed "exit" and quit if they did
		if (!strcmp(userInput[0], "exit") && userInput[1][0] == '\0')
		{
			break;
		}
		//check if the user wants to go to the home directory
		else if ((!strcmp(userInput[0], "cd") && userInput[1][0] == '\0'))
		{
			//change directory to the home directory
			chdir(homedir);
			//update the current working directory
			getcwd(cwd, sizeof(cwd));
		}
		//check if the user typed "cd" followed by a path
		else if (!strcmp(userInput[0], "cd"))
		{
			//check the path
			if (userInput[2][0] == '\0')
			{
				//get the name of the directroy
				char chDir[4096];
				memset(chDir, '\0', sizeof(chDir[0]) * 4096);
				sprintf(chDir, "%s/%s", cwd, userInput[1]);
				//change to the specified directory
				chdir(chDir);
			}
		}
		//check if the user typed "status"
		else if (!strcmp(userInput[0], "status") && userInput[1][0] == '\0')
		{
			//return the exit value of the last process
			if (termSignal != -5)
				printf("Process terminated with signal: %d\n", termSignal);
			else if (exitStatus != -5)
				printf("Process terminated with status: %d\n", exitStatus);
			fflush(stdout);
		}
		//reprompt the user since nothing was entered or a comment was entered
		else if (userInput[0][0] == '\0' || comment == 1)
		{
			//nothing was entered
			if (comment == 1)
			{
				printf("\n");
				fflush(stdout);
			}
		}
		//execute the entered command
		else
		{
			//check for any redirection entered by the user
			int syntax = redirection(userInput, direction);
			//if there was not redirection, resert stdin and stdout
			if (!syntax)
			{
				dup2(direction[0], 0);
				dup2(direction[1], 1);
			}
			//foreground only mode
			if (foreground)
				background = 0;
			//if not in background mode, fork off a child
			if (!background)
			{
				spawnpid = fork();
			}
			//background mode
			else
			{
				//loop through the array, looking for the first place to store a PID
				m = -1;
				do {
					m++;
					if (runningPID[m] == 0)
					{
						//redirect to dev/null if not specified other
						//fork off a child in the background
						spawnpid = fork();
						//set redirection as appropriate
						if (!spawnpid)
						{
							if (direction[0] == 0)
								dup2(devNull, 0);
							if (direction[1] == 1)
								dup2(devNull, 1);
						}
						break;
					}
				} while (runningPID[m] != 0);//loop through the whole array until a empty spot is found
			}
			//check the child process to see if it is a child or the parent
			//this switch statement was taked from the lectures
			switch (spawnpid)
			{
			case -1://error
				perror("Error!");
				exit(1);
				break;
			case 0://this is the child
				if (syntax)
				{
					//error, file does not exist or something
					printf("Error opening file.\n");
					fflush(stdout);
					exit(1);
				}
				//check if the process is a background process
				if (!background) 
				{
					sigint_action.sa_handler = SIG_DFL;
					sigaction(SIGINT, &sigint_action, NULL);
				}
				sigtstp_action.sa_handler = SIG_IGN;
				sigaction(SIGTSTP, &sigtstp_action, NULL);
				//determine the number of argument to send to exec
				j = 0;
				for (i = 0; i < ROWS; i++)
					if (userInput[i][0] != '\0')
						j++;
				for (i = 0; i < j; i++)
					args[i] = &userInput[i][0];
				//close any opened files
				if (direction[0] != 0)
					close(direction[0]);
				if (direction[1] != 1)
					close(direction[1]);
				//execute the process in place of the fork
				if (execvp(*args, args) < 0)
				{
					printf("Error, enter command again\n");
					fflush(stdout);
					exit(1);
				}
				fflush(stdout);
				//redirect as appropriate
				dup2(saved_stdin, 0);
				dup2(saved_stdout, 1);
				exit(0);
				break;
			default://parent process
				//if current process spawned a background process
				if (background)
				{
					runningPID[m] = spawnpid;
					printf("background pid is %d\n", runningPID[m]);
					fflush(stdout);
				}
				//if there are no running background processes
				if (runningPID[0] != 0)
				{
					i = 0;
					//check all running processes and update their status
					while (runningPID[i] != 0)
					{
						childAlive[i] = waitpid(runningPID[i], &childExitStatuses[i], WNOHANG);
						i++;
					}
				}
				//wait until the process is complete
				if(!background)
				{
					actualPid = waitpid(spawnpid, &childExitStatus, 0);
				}
				break;
			}
		}
		//debugging, to show user input
		/*i = 0;
		while (userInput[i][0] != '\0' && i < ROWS)
		{
			//printf("%d ", i);
			for (j = 0; j < COLUMNS; j++)
			{
				printf("%c", userInput[i][j]);
			}
			printf(" ");
			i++;
			fflush(stdout);
		}*/
		//if the user did not enter anything
		if (userInput[0][0] != '\0')
		{
			fflush(stdout);
		}
		//if the user entered a comment
		if (comment == 1)
		{
			fflush(stdout);
		}
		//if the user requested a background process
		else if (background != 0)
		{
			fflush(stdout);
		}
		//clear the user input buffer to the next line
		memset(userInput, '\0', sizeof(userInput[0][0]) * ROWS * COLUMNS);
		//reset the exit status and term signal
		exitStatus = -5;
		termSignal = -5;
		//load the correct child processes exit status or termination signal
		if (WIFEXITED(childExitStatus))
			exitStatus = WEXITSTATUS(childExitStatus);
		else if (WIFSIGNALED(childExitStatus))
			termSignal = WTERMSIG(childExitStatus);
		//check for terminated processes
		m = 0;
		//reset the background exit status and term signal
		backgroundExitStatus = -5;
		backgroundTermSignal = -5;
		//loop through all of the background children
		while (runningPID[m] != 0)
		{
			if (childAlive[m] != 0)
			{
				if (WIFEXITED(childExitStatuses[m]))
				{
					backgroundExitStatus = WEXITSTATUS(childExitStatuses[m]);
					printf("Child PID %d is has exited\n", runningPID[m]);
					break;
				}
				else if (WIFSIGNALED(childExitStatuses[m]))
				{
					backgroundTermSignal = WTERMSIG(childExitStatuses[m]);
					printf("Child PID %d is has terminated\n", runningPID[m]);
					break;
				}
			}
			m++;
		}
		//reset the direction and close the open file
		if (direction[0] != 0)
		{
			close(direction[0]);
			direction[0] = 0;
			dup2(saved_stdin, 0);
		}
		//reset the direction and close the open file
		if (direction[1] != 1)
		{
			close(direction[1]);
			direction[1] = 1;
			dup2(saved_stdout, 1);
		}
	}//end of while loop
	close(devNull);
	return 0;
}