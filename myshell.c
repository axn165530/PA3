// Ashwin Nishad
// axn165530
// CS 3377.0W1 HW3
#include<signal.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include <fcntl.h> //file control options
#include <sys/stat.h> //data returned by stat() function
#include<sys/types.h>
#include<ctype.h>
#include"version.c"

#define MAX_BUF_LENGTH 1024
#define CMD_DELIMITER " \t\n"
#define FNAME_LENGTH 20
static float version = 1.2;
void displayVersion(float);

struct processFields
{
   int pid, pgid;
   char* name;
   int active;
};

int numJobs;
struct processFields table[MAX_BUF_LENGTH];
char user[MAX_BUF_LENGTH];
char baseDirectory[MAX_BUF_LENGTH];
char* pipeCommands[MAX_BUF_LENGTH];
char cwd[MAX_BUF_LENGTH]; // current working directory
char* infile;
char* outfile;
int shell, shell_pgid;
int last, numberPipe;
int piping, inputRedirect, outputRedirect;
char** inputCommandTokens;
char** putputCommandTokens;
char** inputRedirectCommand;
char** outputRedirectCommand;
pid_t my_pid, my_pgid, fgpid;
int is_bg, idxi, idxo;

void getPrompt()
{
   getlogin_r(user, MAX_BUF_LENGTH - 1);
}

void getHomeDir()
{
   getcwd(baseDirectory,  MAX_BUF_LENGTH - 1);
   strcpy(cwd, baseDirectory);
}

void setPrompt()
{
   printf("%s:%s$ ", user, cwd);
}

void modCWD(char* cwd)
{
   int i, j;
   for(i = 0; cwd[i]==baseDirectory[i] && cwd[i]!='\0' && baseDirectory[i] != '\0'; i++);
   if(baseDirectory[i] == '\0')
   {
      cwd[0] = '~';
      for(j = 1; cwd[i]!='\0'; j++)
      {
         cwd[j] = cwd[i++];
      }
      cwd[j] = '\0';
   }
}

void signalHandler(int signum)
{
   if(signum == SIGINT)
   {
      signal(SIGINT,SIG_IGN); // For ignoring ctrl-c
      signal(SIGINT, signalHandler); // For re-setting signal handler
   }
   else if(signum == SIGCHLD) // For handling signal from child processes
   {
      int i, status, die_pid;
      while((die_pid = waitpid(-1, &status, WNOHANG)) > 0)
      {
         for(i = 0; i < numJobs; i++)
         {
            if(table[i].active==0) continue;
            else if(table[i].pid == die_pid)
               break;
         }
         if(i != numJobs)
         {
            if(WIFEXITED(status))
               fprintf(stdout, "\n%s with pid %d exited normally\n", table[i].name, table[i].pid);
            else if(WIFSIGNALED(status))
               fprintf(stdout, "\n%s with pid %d has exited with signal\n", table[i].name, table[i].pid);
            table[i].active = 0;
         }
      }
   }
}

void initialize()
{
   shell = STDERR_FILENO;
   numJobs = 0;
   inputCommandTokens = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH);
   putputCommandTokens = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH);

   if(isatty(shell))
   {
      while(tcgetpgrp(shell) != (shell_pgid = getpgrp()))
         kill(shell_pgid, SIGTTIN);
   }

   signal (SIGINT, SIG_IGN); // ignore ctrl-c
   signal (SIGTSTP, SIG_IGN); // ignore ctrl-z
   signal (SIGQUIT, SIG_IGN); // ignore ctrl
   signal (SIGTTIN, SIG_IGN); // ignore background processes
   signal (SIGTTOU, SIG_IGN);

   my_pid = my_pgid = getpid();
   setpgid(my_pid, my_pgid);
   tcsetpgrp(shell, my_pgid);

   getPrompt();
   getHomeDir();
   modCWD(cwd);
}

void addProcess(int pid, char* name)
{
   table[numJobs].pid = pid;
   table[numJobs].name = strdup(name);
   table[numJobs].active = 1;
   numJobs++;
}

void remProcess(int pid)
{
   int i;
   for(i = 0 ; i < numJobs; i++)
   {
      if(table[i].pid == pid)
      {
         table[i].active = 0;
         break;
      }
   }
}

int runCommand(char** commandTokens)
{
   pid_t pid;
   pid = fork();
   if(pid < 0)
      {
         perror("Child Proc. not created\n");
         return -1;
      }
   else if(pid==0)
      {
         int fin, fout;
         setpgid(pid, pid);

         if(inputRedirect)
            {
               fin = openInFile();
               if(fin == -1) _exit(-1);
            }
         if(outputRedirect)
            {
               fout = openOutFile();
               if(fout == -1) _exit(-1);
            }

         if(is_bg == 0) tcsetpgrp(shell, getpid());
           
         signal (SIGINT, SIG_DFL);
         signal (SIGQUIT, SIG_DFL);
         signal (SIGTSTP, SIG_DFL);  //suspend a process
         signal (SIGTTIN, SIG_DFL);
         signal (SIGTTOU, SIG_DFL);
         signal (SIGCHLD, SIG_DFL);
         int ret;
         if((ret = execvp(commandTokens[0], commandTokens)) < 0)
         {
            perror("Error executing command!\n");
            _exit(-1);
         }
            _exit(0);
        }
        if(is_bg == 0)
        {
           tcsetpgrp(shell, pid);
           addProcess(pid, commandTokens[0]);
           int status;
           fgpid = pid;
           waitpid(pid, &status, WUNTRACED);

           if(!WIFSTOPPED(status)) remProcess(pid);

           else fprintf(stderr, "\n%s with pid %d has stopped!\n", commandTokens[0], pid);

           tcsetpgrp(shell, my_pgid);
        }
        else
        {
           printf("\[%d] %d\n", numJobs, pid);
           addProcess(pid, commandTokens[0]);
           return 0;
        }
}

int cdCMD(char** commandTokens, char* cwd, char* baseDirectory)
{
   if(commandTokens[1] == NULL || strcmp(commandTokens[1], "~\0") == 0 || strcmp(commandTokens[1], "~/\0") == 0)
   {
      chdir(baseDirectory);
      strcpy(cwd, baseDirectory);
      modCWD(cwd);
   }
   else if(chdir(commandTokens[1]) == 0)
   {
      getcwd(cwd, MAX_BUF_LENGTH);
      modCWD(cwd);
      return 0;
   }
   else
   {
      perror("Error executing cd command");
   }
}

void echo(char** commandTokens, int tokens, char* cmd)
{
   if(tokens > 1 && commandTokens[1][0] == '-')
   {
      runCommand(commandTokens);
      return;
   }
   int i, len = 0, in_quote = 0, flag = 0;
   char buf[MAX_BUF_LENGTH] = "\0";
   for(i = 0; isspace(cmd[i]); i++);
   if(i == 0) i = 5;
      else i += 4;
   for(; cmd[i] != '\0' ; i++)
   {
      if(cmd[i] == '"')
      {
         in_quote = 1 - in_quote;
      }
      else if(in_quote == 0 && (isspace(cmd[i])) && flag == 0)
      {
         flag = 1;
         if(len > 0) buf[len++] = ' ';
      }
      else if(in_quote == 1 || !isspace(cmd[i])) buf[len++] = cmd[i];
         if(!isspace(cmd[i]) && flag == 1) flag = 0;
   }
   if(in_quote == 1)
   {
      fprintf(stderr, "Missing quotes\n");
      return;
   }
   else printf("%s\n", buf);
}

void pwd(char** commandTokens)
{
   char pwd_dir[MAX_BUF_LENGTH];
   getcwd(pwd_dir, MAX_BUF_LENGTH - 1);
   if(commandTokens[1] == NULL) printf("%s\n", pwd_dir);
   else runCommand(commandTokens);
}

void jobs()
{
   int i;
   for(i = 0; i < numJobs ; i++)
   {
      if(table[i].active == 1)
      {
         printf("[%d] %s [%d]\n", i, table[i].name, table[i].pid);
      }
   }
}

int openInFile()
{
   int f = open(infile, O_RDONLY, S_IRWXU);
   if (f < 0)
   {
      perror(infile);
   }
   dup2(f, STDIN_FILENO);
   close(f);
   return f;
}

int openOutFile()
{
   int f;
   if(last == 1) f = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
   else if(last == 2) f = open(outfile, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
   if(f < 0)
   {
      perror(outfile);
   }
   dup2(f, STDOUT_FILENO);
   close(f);
   return f;
}

// Function to read the command entered
char* readCommandLine()
{
   int len=0,c;
   char* cmd = malloc(sizeof(char)*MAX_BUF_LENGTH);
   while(1)
   {
      c = getchar();
      if(c == '\n')
      {
         cmd[len++] = '\0';
         break;
      }
      else
         cmd[len++] = c;
   }
   return cmd;
}

int parseCommandLine(char* cmdLine, char** cmds)
{
   int numberOfCommands = 0;
   char* token = strtok(cmdLine, ";");
   while(token!=NULL)
   {
      cmds[numberOfCommands++] = token;
      token = strtok(NULL, ";");
   }
   return numberOfCommands;
}

int parseCommand(char* cmd, char** commandTokens)
{
   int tok = 0;
   char* token = strtok(cmd, CMD_DELIMITER);
   while(token!=NULL)
   {
      commandTokens[tok++] = token;
      token = strtok(NULL, CMD_DELIMITER);
   }
   return tok;
}
// Function to check if command contains pipe
int checkPipe(char* cmd)
{
   int i;
   idxi = idxo = last = piping = inputRedirect = outputRedirect = 0;
   for( i = 0 ; cmd[i] ; i++)
   {
      if(cmd[i] == '|')
      {
         piping = 1;
      }
      if(cmd[i] == '<')
      {
         inputRedirect = 1;
         if(idxi == 0 ) idxi = i;
      }
      if(cmd[i] == '>')
      {
         outputRedirect = 1;
         if(last == 0) last = 1;
         if(idxo == 0 ) idxo = i;
      }
      if(cmd[i] == '>' && cmd[i+1] == '>') last = 2;
   }
   if(piping) return 1;
   else return -1;
}
//
void parsePiping(char* cmd)
{
   char* copy = strdup(cmd);
   char* token;
   int tok = 0;
   token = strtok(copy, "|");
   while(token!= NULL)
   {
      pipeCommands[tok++] = token;
      token = strtok(NULL, "|");
   }
   numberPipe = tok;
}
//
int parseRedirect(char* cmd, char** commandTokens)
{
   char* copy = strdup(cmd);
   idxi = idxo = last = inputRedirect = outputRedirect = 0;
   infile = outfile = NULL;
   int i, tok = 0;
   for( i = 0 ; cmd[i] ; i++)
   {
      if(cmd[i] == '<')
      {
         inputRedirect = 1;
         if(idxi == 0 ) idxi = i;
      }
      if(cmd[i] == '>')
      {
         outputRedirect = 1;
         if(last == 0) last = 1;
         if(idxo == 0 ) idxo = i;
      }
      if(cmd[i] == '>' && cmd[i+1] == '>') last = 2;
   }
   if(inputRedirect == 1 && outputRedirect == 1)
   {
      char* token;
      token = strtok(copy, " <>\t\n");
      while(token!=NULL)
      {
         commandTokens[tok++] = strdup(token);
         token = strtok(NULL, "<> \t\n");
      }
      if(idxi < idxo )
      {
         infile = strdup(commandTokens[tok - 2]);
         outfile = strdup(commandTokens[tok - 1]);
      }
      else
      {
         infile = strdup(commandTokens[tok - 1]);
         outfile = strdup(commandTokens[tok - 2]);
      }
      commandTokens[tok - 2] = commandTokens[tok - 1] = NULL;
      return tok - 2;
   }

   if(inputRedirect == 1)
   {
      char* token;
      char* copy = strdup(cmd);
      char** inputRedirectCommand = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH);
      token = strtok(copy, "<");
      while(token!=NULL)
      {
         inputRedirectCommand[tok++] = token;
         token = strtok(NULL, "<");
      }
      copy = strdup(inputRedirectCommand[tok - 1]);

      token = strtok(copy, "> |\t\n");
      infile = strdup(token);

      tok = 0;
      token = strtok(inputRedirectCommand[0], CMD_DELIMITER);
      while(token!=NULL)
      {
         commandTokens[tok++] = strdup(token);
         token = strtok(NULL, CMD_DELIMITER);
      }

      commandTokens[tok] = NULL;
      free(inputRedirectCommand);
   }

   if(outputRedirect == 1)
   {
      char* copy = strdup(cmd);
      char* token;
      char** outputRedirectCommand = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH);
      if(last == 1)
         token = strtok(copy, ">");
      else if(last == 2)
         token = strtok(copy, ">>");
      while(token!=NULL)
      {
         outputRedirectCommand[tok++] = token;
         if(last == 1) token = strtok(NULL, ">");
         else if(last == 2) token = strtok(NULL, ">>");
      }

      copy = strdup(outputRedirectCommand[tok - 1]);
      token = strtok(copy, "< |\t\n");
      outfile = strdup(token);
      tok = 0;
      token = strtok(outputRedirectCommand[0], CMD_DELIMITER);
      while(token!=NULL)
      {
         commandTokens[tok++] = token;
         token = strtok(NULL, CMD_DELIMITER);
      }
      free(outputRedirectCommand);
   }
   if(inputRedirect == 0 && outputRedirect == 0 ) return parseCommand(strdup(cmd), commandTokens);
   else return tok;
}

void normalCommand(int tokens, char** commandTokens, char* cmdCopy)
{
   if(tokens > 0)
   {
      if(strcmp(commandTokens[0], "jobs\0") == 0) jobs();
      else if(strcmp(commandTokens[tokens-1], "&\0") == 0)
      {
         commandTokens[tokens - 1] = NULL;
         is_bg = 1;
         runCommand(commandTokens);
      }
      else if(strcmp(commandTokens[0], "cd\0") == 0) cdCMD(commandTokens, cwd, baseDirectory);
      else if(strcmp(commandTokens[0], "pwd\0") == 0) pwd(commandTokens);
      else if(strcmp(commandTokens[0], "quit\0") == 0)
      {
         _exit(0);
      }
      else if(strcmp(commandTokens[0], "echo\0") == 0) echo(commandTokens, tokens, cmdCopy);
      else if(isalpha(commandTokens[0][0]))
         runCommand(commandTokens);
   }
   free(commandTokens);
}
//
void redirectPipingCommand(char* cmd)
{
   int pid, pgid, fin, fout;
   numberPipe = 0;
   parsePiping(cmd);
   int* pipes = (int* )malloc(sizeof(int)*(2*(numberPipe - 1)));
   int i;
   for(i = 0; i < 2*numberPipe - 3; i += 2)
   {
      if(pipe(pipes + i) < 0 )
      {
         perror("Pipe not opened!\n");
         return;
      }
   }
   int status,j;
   for(i = 0; i < numberPipe ; i++)
   {
      char** commandTokens = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH); // array of command tokens
      int tokens = parseRedirect(strdup(pipeCommands[i]), commandTokens);
      is_bg = 0;
      pid = fork();
      if(i < numberPipe - 1)
         addProcess(pid, commandTokens[0]);
      if(pid != 0 )
      {
         if(i == 0 ) pgid = pid;
         setpgid(pid, pgid);
      }
      if(pid < 0)
      {
         perror("Fork Error!\n");
      }
      else if(pid == 0)
      {
         signal (SIGINT, SIG_DFL);
         signal (SIGQUIT, SIG_DFL);
         signal (SIGTSTP, SIG_DFL);
         signal (SIGTTIN, SIG_DFL);
         signal (SIGTTOU, SIG_DFL);
         signal (SIGCHLD, SIG_DFL);
         if(outputRedirect) fout = openOutFile();
         else if(i < numberPipe - 1) dup2(pipes[2*i + 1], 1);
         if(inputRedirect) fin = openInFile();
         else if(i > 0 ) dup2(pipes[2*i -2], 0);
         int j;
         for(j = 0; j < 2*numberPipe - 2; j++) close(pipes[j]);
         if(execvp(commandTokens[0], commandTokens) < 0 )
         {
            perror("Execvp error!\n");
            _exit(-1);
         }
      }
   }

   for(i = 0; i < 2*numberPipe - 2; i++) close(pipes[i]);
   if(is_bg == 0)
   {
      tcsetpgrp(shell, pgid);
      for(i = 0; i < numberPipe ; i++)
      {
         int cpid = waitpid(-pgid, &status, WUNTRACED);
         if(!WIFSTOPPED(status)) remProcess(cpid);
      }
      tcsetpgrp(shell, my_pgid);
   }
}
//

int main()
{
   // Setup
   initialize();
   displayVersion(version);
   // will run forever until quit is entered
   while(1)
   {
      if(signal(SIGCHLD,signalHandler)==SIG_ERR)
         perror("can't catch SIGCHLD");
      if(signal(SIGINT,signalHandler)==SIG_ERR)
         perror("can't catch SIGINT!");
      setPrompt();
      int i,j;
      char** cmds = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH); // semi-colon separated commands

      for(j = 0; j < MAX_BUF_LENGTH; j++)
         cmds[j] = '\0';

      char* cmdLine = readCommandLine(); // read the command line
      int numberOfCommands = parseCommandLine(cmdLine, cmds); // parse the command line

      for(i = 0; i < numberOfCommands; i++)
      {
         infile = outfile = NULL;
         is_bg = 0, numberPipe = 0;
         char* cmdCopy = strdup(cmds[i]);

         char** commandTokens = malloc((sizeof(char)*MAX_BUF_LENGTH)*MAX_BUF_LENGTH);
         for(j = 0; j < MAX_BUF_LENGTH; j++)
            commandTokens[j] = '\0';

         if(checkPipe(strdup(cmds[i])) == -1)
         {
            if(inputRedirect == 1 || outputRedirect == 1) normalCommand(parseRedirect(strdup(cmdCopy), commandTokens), commandTokens, cmdCopy);
            else
            {
               int tokens = parseCommand(strdup(cmds[i]), commandTokens);
               normalCommand(tokens, commandTokens, cmdCopy);
            }
         }
         else redirectPipingCommand(cmds[i]);
      }
      if(cmds) free(cmds);
      if(cmdLine) free(cmdLine);
   }
   return 0;
}
