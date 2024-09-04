//*********************************************************
//
// Matthew Bolding
// Operating Systems
// Project #1: Writing Your Own Shell: hfsh
//
//*********************************************************


//*********************************************************
//
// Includes and Defines
//
//*********************************************************
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <map>
#include <list>
#include <string>
#include <fcntl.h>
#include <time.h>

#define STR_MYEXIT "myexit"

#define MAXLINE   1024
#define MAXJOBS   256
#define MAXJID    1 << 16

#define UNDEF     0
#define FG        1
#define BG        2
#define ST        3

#define READ_END  0
#define WRITE_END 1

//*********************************************************
//
// Structure Declarations
//
//*********************************************************

struct job_t {
    pid_t pid;
    int jid;
    int state;
    std::string cmdline;
};

struct fs_elem {
    char *name;
    char *color;
} element;

struct piped {
    char *file_in;
    char *file_out;
    char **command;
    int file_in_fd;
    int file_out_fd;
    int p_fd[2] = {0, 1};
} piped_command;

struct job_t jobs[MAXJOBS];

//*********************************************************
//
// Type Declarations
//
//*********************************************************

using namespace std;

//*********************************************************
//
// Extern Declarations
//
//*********************************************************

extern "C"
{
  extern char **gettoks();
} 

//*********************************************************
//
// Function Prototypes
//
//*********************************************************

// Signal Handlers
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void sighup_handler(int sig);
void sigchld_handler(int sig);

typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

// Job Handeling (copied from CSF tsh)
int pid2jid(pid_t pid);
pid_t fgpid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state);
int maxjid(struct job_t *jobs);
void waitfg(pid_t pid);
int deletejob(struct job_t *jobs, pid_t pid);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
void clearjob(struct job_t *job);

// Exiting the shell
void myexit();

// Functions related to myhist
int myhist();
string current_command();
void update_history(char **toks);

// Functions related to forweb
int forweb(char *argv[]);
int forweb_worker(char *dir_name);

// Functions related to prunedir
int prune_dir(char *argv[]);
int prune_dir_worker(char *dir_name);

// Functions related to nls
int nls(char *argv[]);
int get_contents(string folder_name, list<fs_elem> *files, list<fs_elem> *folders);
int list_files(list<fs_elem> *files);
int list_dirs(list<fs_elem> *folders);

void refresh_prompt();

// Functions related to evaluating and executing the command
int evaluate_cmd();
void parse_tokens(char **argv);
int external_cmd();
void reset_variables();
void parent_tasks(pid_t pid);

// Functions related to < and > operations
void setup_redirection(list<piped>::iterator iterator);
void setup_stdout(int *fd, char *file);
void setup_stdin(int *fd, char *file);
void exec_wrapper(list<piped>::iterator iterator);
void execute_pipe(int in, int out, list<piped>::iterator iterator);

void print_signal_table();

//*********************************************************
//
// Global Variables
//
//*********************************************************

// history map keeps track of the commands passed to the shell
map<int, string> history;

// pipe_commands is a list of commands that pipe together;
// even if one command, without a pipe, is executed, this
// variable is still updated
list<piped> pipe_commands;

// first_com and last_com are used when determining what command
// leads and follows a series of piped commands
char **first_com;
char **last_com;

// job variables
int mode;
int nextjid = 1;

// c_{int, tstp, hup, quit} tabluate the number of times the shell
// receives various signals
unsigned int c_int = 0;
unsigned int c_tstp = 0;
unsigned int c_hup = 0;
unsigned int c_quit = 0;

// constants for command parsing and colorful printing
char AMPERSAND[] = "&";
char IN_REDIR[] = "<";
char OUT_REDIR[] = ">";
char PIPE[] = "|";

char red[] = "\u001b[31m";
char green[] = "\u001b[32m";
char blue[] = "\u001b[34m";
char purple[] = "\u001b[35m";
char gray[] = "\u001b[90m";
char reset[] = "\u001b[0m";
char bold[] = "\u001b[1m";

//*********************************************************
//
// Main Function
//
//*********************************************************

int main(int argc, char *argv[])
{
    int retval = 0;
    char **toks = NULL;

    // Set up the signal handlers
    Signal(SIGHUP, sighup_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGQUIT, sigquit_handler);
    Signal(SIGTSTP, sigtstp_handler);
    Signal(SIGCHLD, sigchld_handler);

    // Get the prompt
    refresh_prompt();

    while(true) {
        toks = gettoks();

        if(toks[0] != NULL) {
            // Update the history
            update_history(toks);
            
            // Exit, if the exit string is passed
            if(!strcmp(toks[0], STR_MYEXIT)) break;

            // Determine how to treat the function            
            parse_tokens(toks);

            // Execute the command
            evaluate_cmd();
        }
        // Reset instance variables, such as the struct piped_command
        reset_variables();
        
        refresh_prompt();
    }

    print_signal_table();

    return(retval);
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig)
{
    pid_t pid = fgpid(jobs);

    if (pid != 0)
    {
        // Kill the foreground process, if one exists.
        kill(-pid, sig);
    }

    c_tstp++;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig)
{
    pid_t pid = fgpid(jobs);
    
    if (pid != 0)
    {
        // Kill the foreground process, if one exists.
        kill(-pid, sig);
    }

    c_int++;
    return;
}

/*
 * sighup_handler - A signal handler for SIGHUP.
 *
 */
void sighup_handler(int sig) {
    c_hup++;
    return;
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    c_quit++;
    print_signal_table();
    exit(1);
}

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig)
{
    // Declare pid and status variables.
    pid_t pid;
    int status;

    // While any valid child processes exit,
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        struct job_t *current_job = getjobpid(jobs, pid);

        // If the process is stopped by ctrl-z, for example,
        if (WIFSTOPPED(status))
        {
            // set the state to stopped,
            current_job->state = ST;
            // and print out that the job was stopped by a signal SIGTSTP.
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, SIGTSTP);
        }
        // If the process is terminated by ctrl-c, for example,
        else if (WIFSIGNALED(status))
        {
            // print out that the job was terminated by a signal, WTERMSIG(status),
            printf("Job [%d] (%d) terminated by signal %d\n", current_job->jid, current_job->pid, WTERMSIG(status));
            // and remove the job.
            deletejob(jobs, pid);
        }
        // For completed jobs,
        else if (WIFEXITED(status))
        {
            // remove the job.
            deletejob(jobs, pid);
        }
    }
    return;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG) {
            return jobs[i].pid;
        }       
    }
    return 0;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // Get the job list.
    struct job_t *job = getjobpid(jobs, pid);

    // If the parent calls waitfg,
    if (pid == 0)
    {
        // return;
        return;
    }
    // otherwise, if the pid is greater than 0,
    else if (job != NULL)
    {
        // while the foreground job is still running,
        while (pid == fgpid(jobs))
        {
            // use sleep(1) to implement a busy loop.
            sleep(1);
        }
    }
    return;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            // Return a job from its PID
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {  
            // Return a job id based on the PID
            return jobs[i].jid;
        }
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            // Clear the job that has pid_t pid
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, string cmd)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            // Set the job's fields
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            
            jobs[i].cmdline = cmd;
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/*
 * parse_toks, given a char** structured like toks, will iterate through the array
 * and find instances of <, >, & and |. Upon finding various key tokens, those locations
 * are noted, and their entry in the array is set to NULL, to prevent interpretation by
 * execvp. File in, out, and the command itself is stored as a struct and pushed to the
 * pipe_commands global variale.
 */
void parse_tokens(char **argv) {
    int i = 0;
    string temp;

    piped_command.command = &argv[0];

    for(i = 0; argv[i] != NULL; i++) {
        temp = argv[i];
        if(temp == IN_REDIR) {
            // We have found a file in.
            // Note the file, and make the index set to NULL.
            piped_command.file_in = argv[i + 1];
            argv[i] = NULL;

        } else if(temp == OUT_REDIR) {
            // We have found a file out.
            // Note the file, and make the index set to NULL.
            piped_command.file_out = argv[i + 1];
            argv[i] = NULL;

        } else if(temp == PIPE) {
            // We have found a pipe.
            // Push the current working element, and reset fields
            // to allow the next command to populate a default 
            // pipe_command struct.
            pipe_commands.push_back(piped_command);
            piped_command.command = &argv[i + 1];
            piped_command.file_in = NULL;
            piped_command.file_out = NULL;
            argv[i] = NULL;

         }else if(argv[i + 1] == NULL) {
            // At the end, determien if there's a &.
            temp = argv[i];
            if(temp == AMPERSAND) {
                mode = BG;
                argv[i] = NULL;
            } else {
                mode = FG;
            }
        }
    }

    // Push the final element.
    pipe_commands.push_back(piped_command);
}

/* 
 * evaluate_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int evaluate_cmd()
{
    // Look at the first command to determine how to proceed
    char **argv = pipe_commands.front().command;

    if(!strcmp(argv[0], "myhist")) {
        return myhist();
    }
    else if(!strcmp(argv[0], "forweb")) {
        return forweb(argv);
    }
    else if(!strcmp(argv[0], "nls")) {
        return nls(argv);
    }
    else if(!strcmp(argv[0], "prunedir")) {
        return prune_dir(argv);
    }
    else {
        return external_cmd();
    }
}

/* 
 * external_cmd - If the user has typed an external command then execute
 *    it immediately.  
 */
int external_cmd() {
    int in = 0;
    pid_t pid;

    list<piped>::iterator iterator;

    // Note the first, and last commands.
    // These may be the same!
    first_com = pipe_commands.front().command;
    last_com = pipe_commands.back().command;

    for(iterator = pipe_commands.begin(); iterator != pipe_commands.end(); iterator++) {
        // If there's only one element in the pipe_commands list, there will not be
        // a pipe. So fork(), and proceed normally.
        if(pipe_commands.size() == 1) {
            if ((pid = fork()) < 0) {
                fprintf(stderr, "%s\n", "fork() encountered an error");
            } else if(pid == 0) {
                // Set up redirections!
                setup_redirection(iterator);
                exec_wrapper(iterator);
            } else {
                addjob(jobs, pid, mode, current_command());
            }

            parent_tasks(pid);
        } else {
            // Otherwise, there's more than one piped command.
            // Set up pipes.
            pipe((*iterator).p_fd);

            if((*iterator).command != last_com) {
                execute_pipe(in, (*iterator).p_fd[WRITE_END], iterator);
            } else {
                // The last command will have an out FD of 1,
                // unless changed by setup_redirection, later.
                execute_pipe(in, STDOUT_FILENO, iterator);
            }

            // Close the unneeded pipe end.
            close((*iterator).p_fd[WRITE_END]);

            // Note the pipe the next command will use at its stdin.
            in = (*iterator).p_fd[READ_END];
        }   
    }

    return 0;
}

/*
 * exec_wrapper - a wrapper function to execute a command from an iterator position
 */
void exec_wrapper(list<piped>::iterator iterator) {
    if (execvp((*iterator).command[0], (*iterator).command) < 0)
    {
        printf("%s: command not found.\n", (*iterator).command[0]);
        exit(1);
    }
}

/*
 * execute_pipe - set up a pipe from in, out file descriptors,
 * and execute the command from iterator.
 */
void execute_pipe(int in, int out, list<piped>::iterator iterator) {
    pid_t pid;

    if ((pid = fork()) < 0) {
        fprintf(stderr, "%s\n", "fork() encountered an error");
    } 
    else if (pid == 0) {
        // Configure the correct in and out of the new child
        if(in != 0) {
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if(out != 1) {
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        // Set up file redirection. This will overwrite pipes!
        setup_redirection(iterator);
        exec_wrapper(iterator);
    } else {
        addjob(jobs, pid, mode, current_command());
    }

    parent_tasks(pid);
}

/*
 * parent_tasks - a helper function to break out the duties of the parent process:
 * either wait for the task, or let it run.
 */
void parent_tasks(pid_t pid) {
    if (mode == FG) {
        waitfg(pid);
    } else {
        printf("[%d] (%d) %s", pid2jid(pid), pid, current_command().c_str());
    }
}

/*
 * setup_redirection - a helper function to orchestrate setting up file redirection
 */
void setup_redirection(list<piped>::iterator iterator) {
    if((*iterator).file_in != NULL && (*iterator).file_out != NULL) {
        // Both File In and Out
        setup_stdout(&(*iterator).file_out_fd, (*iterator).file_out);
        setup_stdin(&(*iterator).file_in_fd, (*iterator).file_in);

    } else if((*iterator).file_in != NULL && (*iterator).file_out == NULL) {
        // File In
        setup_stdin(&(*iterator).file_in_fd, (*iterator).file_in);

    } else if((*iterator).file_in == NULL && (*iterator).file_out != NULL) {
        // File Out
        setup_stdout(&(*iterator).file_out_fd, (*iterator).file_out);
    }
}

/*
 * setup_stdout - a helper function to configure the proper stdout of a process
 */
void setup_stdout(int *fd, char *file) {
    // Use a default mode
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    // Make the file, and close STDOUT
    *fd = creat(file, mode);
    close(STDOUT_FILENO);
    dup(*fd);
    close(*fd);
}

/*
 * setup_stdin - a helper function to configure the proper stdin of a process
 */
void setup_stdin(int *fd, char *file) {
    // Try to open the file
    *fd = open(file, O_RDONLY);
    close(STDIN_FILENO);

    // Make the file itself the child's stdin
    dup(*fd);
    close(*fd);
}

/*
 * reset_variables - a helper function to reset global variables and
 * prepare for the next command
 */
void reset_variables() {
    piped_command.command = NULL;
    piped_command.file_in = NULL;
    piped_command.file_out = NULL;
    piped_command.file_in_fd = -1;
    piped_command.file_out_fd = -1;
    pipe_commands.clear();
} 

/*
 * refresh_prompt - a function to get and print a new prompt
 */
void refresh_prompt() {
    char p_time[64];
    char p_username[64];

    // Add data and time information.
    time_t now = time(NULL);
    struct tm l_time = *localtime(&now);
    sprintf(p_time, "%s[%02d/%02d/%d %02d:%02d:%02d]%s ", bold, l_time.tm_mon + 1, l_time.tm_mday, l_time.tm_year + 1900, l_time.tm_hour, l_time.tm_min, l_time.tm_sec, reset);

    // Add user information.
    char *username = getlogin();
    sprintf(p_username, "%s%s%s > ", purple, username, reset);

    // Print the two parts of the prompt.
    printf("%s%s", p_time, p_username);
}

/*
 * myhist - prints the history map in order
 */
int myhist() {
    map<int, string>::iterator iterator;

    for(iterator = history.begin(); iterator != history.end(); iterator++) {
        std::cout << " " << iterator->first << " " << iterator->second << std::endl;
    }

    return 0;
}

/*
 * myhist - returns the most recent (current) command as a string
 */
string current_command() {
    map<int, string>::iterator iterator;

    iterator = history.end();
    iterator--;

    return iterator->second;
}

/*
 * update_history - updates the history map
 */
void update_history(char** toks) {
    int i;
    string command;
    
    if(toks[0] != NULL) {
        for(i = 0; toks[i] != NULL; i++) {
            // Piece together the string
            command = command + " " + toks[i];
        }
    }

    // Add it to the map with an integer one more than the current size
    history.insert({history.size() + 1, command});
}

/*
 * nls - given a directory, list all files, displaying their types
 */
int nls(char *argv[]) {
    // Maintain a list of folders and elements
    char path[256];
    list<fs_elem> files;
    list<fs_elem> folders;
    
    if(argv[1] != NULL) {
        for(int i = 1; argv[i] != NULL; i++) {
            // Make sure the parameter is a directory---not a file
            if(opendir(argv[i]) == NULL) {
                fprintf(stderr, "%s%s%s\n", "nls: cannot access '", argv[i], "': No such directory");
                return 1;
            }
            
            // Find the path
            realpath(argv[i], path);
            fprintf(stdout, "%s%s\n", argv[i], ":");

            // Use path to determine the contents of the folder
            get_contents(path, &files, &folders);
            // List the contents
            list_dirs(&folders); list_files(&files);

            // Clear the lists to prepare for the next directory, if one exists
            folders.clear(); files.clear();
            if(argv[i + 1] != NULL) {
                fprintf(stdout, "\n\n");
            } else {
                fprintf(stdout, "\n");
            }
        }
    } else {
        // If no parameter is passed, look in the current directory
        // and follow the same procedure as above
        realpath(".", path);
        fprintf(stdout, "%s%s\n", ".", ":");
        get_contents(".", &files, &folders);
        list_dirs(&folders); list_files(&files);
        fprintf(stdout, "\n");
    }
    
    return 0;
}

/*
 * get_contents - given a directory, find the files and the folders
 */
int get_contents(string location, list<fs_elem> *files, list<fs_elem> *folders) {
    DIR *directory = opendir(location.c_str());
    struct dirent *directory_entry;
    struct stat file_stat;

    // Open the directory
    while((directory_entry = readdir(directory)) != 0) {
        char fq_path[512];
        sprintf(fq_path, "%s/%s", location.c_str(), directory_entry->d_name);
        lstat(fq_path, &file_stat);

        // Do not look for hidden files---ones that start with .
        if(directory_entry->d_name[0] != '.')
        {
            element.name = directory_entry->d_name;

            // Determine whether the item is a directory
            if(S_ISDIR(file_stat.st_mode)) {
                element.color = blue;
                (*folders).push_back(element);

            // a symbolic link
            } else if(S_ISLNK(file_stat.st_mode)) {
                element.color = red;
                (*files).push_back(element);

            // an executable
            } else if(file_stat.st_mode & S_IXUSR || file_stat.st_mode & S_IXGRP || file_stat.st_mode & S_IXOTH) {
                element.color = green;
                (*files).push_back(element);

            // or simply a normal file
            } else {
                element.color = gray;
                (*files).push_back(element);
            }
        }
    }
    
    closedir(directory);
    return 0;
}

/*
 * list_files - given a list of fs_elements---files, in this case---print them
 */
int list_files(list<fs_elem> *files) {
    list<fs_elem>::iterator iterator;

    for(iterator = files->begin(); iterator != files->end(); iterator++) {
        fprintf(stdout, "%s%s%s ", iterator->color, iterator->name, reset);
    }

    return 0;
}

/*
 * list_dirs - given a list of fs_elements---directories, in this case---print them
 */
int list_dirs(list<fs_elem> *folders) {
    list<fs_elem>::iterator iterator;

    for(iterator = folders->begin(); iterator != folders->end(); iterator++) {
        fprintf(stdout, "%s%s%s ", iterator->color, iterator->name, reset);
    }

    return 0;
}

/*
 * forweb - given a directory, call the worker function
 */
int forweb(char *argv[]) {
    if(argv[1] != NULL) {
        return forweb_worker(argv[1]);
    } else {
        return forweb_worker((char *) "."); 
    }
}

/*
 * forweb_worker - works on a directory to recursively add permissions to files and folders
 */
int forweb_worker(char *dir_name) {
    DIR *directory;

    // Check if the directory exists
    if((directory = opendir(dir_name)) == NULL) {
        fprintf(stderr, "%s%s%s\n", "forweb: cannot access '", dir_name, "': No such directory");
        return 2;
    }

    struct dirent *directory_entry;
    struct stat file_stat;
    
    // Iterate over all directory entries
    while((directory_entry = readdir(directory)) != 0) {
        char fq_path[512];
        sprintf(fq_path, "%s/%s", dir_name, directory_entry->d_name);
        stat(fq_path, &file_stat);

        // If the entry if a directory, and not .. or ., recursively call the function to enter that directory
        if(S_ISDIR(file_stat.st_mode)) {
            if(strcmp(directory_entry->d_name, ".") != 0 && strcmp(directory_entry->d_name, "..") != 0) {
                // Modify its permissions
                chmod(fq_path, file_stat.st_mode | S_IROTH | S_IXOTH);
                forweb_worker(fq_path);
            }
        } else {
            // Otherwise, it's a file. Modify the permissions appropriately
            chmod(fq_path, file_stat.st_mode | S_IROTH);
        }
    }
    
    // Close the directory
    closedir(directory);
    return 0;
}

/*
 * prune_dir - given a directory, call the worker function
 */
int prune_dir(char *argv[]) {
    if(argv[1] != NULL) {
        return prune_dir_worker(argv[1]);
    } else {
        return prune_dir_worker((char *) ".");
    }
}

/*
 * prune_dir_worker - works on a directory to recursively delete all empty files
 */
int prune_dir_worker(char *dir_name) {
    DIR *directory;

    // Check if the directory exists
    if((directory = opendir(dir_name)) == NULL) {
        fprintf(stderr, "%s%s%s\n", "prunedir: cannot access '", dir_name, "': No such directory");
        return 2;
    }

    struct dirent *directory_entry;
    struct stat file_stat;
    
    // Iterate over all directory entries
    while((directory_entry = readdir(directory)) != 0) {
        char fq_path[512];
        sprintf(fq_path, "%s/%s", dir_name, directory_entry->d_name);
        stat(fq_path, &file_stat);

        // If the entry if a directory, and not .. or ., recursively call the function to enter that directory
        if(S_ISDIR(file_stat.st_mode)) {
            if(strcmp(directory_entry->d_name, ".") != 0 && strcmp(directory_entry->d_name, "..") != 0) {
                prune_dir_worker(fq_path);
            }
        } else {
            // Otherwise, the entry is a file. Check its size.
            if(file_stat.st_size == 0) {
                // Delete if it's empty
                if(remove(fq_path) != 0) {
                    fprintf(stderr, "%s%s%s\n", "prunedir: cannot delete '", fq_path, "'\n");
                }
            }
        }
    }
    
    // Close the directory
    closedir(directory);
    return 0;
}

/*
 * print_signal_table - print the number of times a signal was received and subsequently handled
 */
void print_signal_table() {
    fprintf(stdout, "%s%s %s%s\n", purple, "hfsh", reset, "Signals Received");
    fprintf(stdout, "%s%u\n", "SIGINT  : ", c_int);
    fprintf(stdout, "%s%u\n", "SIGQUIT : ", c_quit);
    fprintf(stdout, "%s%u\n", "SIGHUP  : ", c_hup);
    fprintf(stdout, "%s%u\n", "SIGTSTP : ", c_tstp);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        fprintf(stderr, "%s\n", "Signal error");
    return (old_action.sa_handler);
}