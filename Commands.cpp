#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdexcept>
#include <string>

#include <iterator>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


//-----------------------------------------------CommandUtils-----------------------------------------------

char **extractArguments(const char *cmd_line, int *argc) {
    // Create a copy of the command line to safely modify it
    char cmd[COMMAND_MAX_LENGTH];
    strncpy(cmd, cmd_line, COMMAND_MAX_LENGTH);
    cmd[COMMAND_MAX_LENGTH - 1] = '\0'; // Ensure null termination

    // Remove the background sign if it exists
    _removeBackgroundSign(cmd);

    // Allocate memory for the arguments array
    char **args = (char **)malloc((COMMAND_MAX_ARGS + 1) * sizeof(char*));
    if (!args) {
        cerr << "smash error: malloc failed" << endl;
        return nullptr;
    }

    // Initialize the arguments array to avoid undefined behavior
    for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++) {
        args[i] = nullptr;
    }

    // Parse the command line into arguments
    *argc = _parseCommandLine(cmd, args);

    return args;
}

void deleteArguments(char **args)
{
  // Delete each argument in args:
  for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++)
  {
    free(args[i]);
  }
  // Delete args itself:
  free(args);
}

//-----------------------------------------------Command-----------------------------------------------

Command::Command(const char *cmd_line) {
  m_cmd_line = (char*)malloc(strlen(cmd_line) + 1);
  if (m_cmd_line != nullptr) {
    strcpy(m_cmd_line, cmd_line);
  }
}

Command::~Command() {
  free(m_cmd_line);
}

void Command::initialCurrDir()
{
  SmallShell &smash = SmallShell::getInstance();
  char *buffer = (char *)malloc((PATH_MAX + 1) * sizeof(char));
  if (!buffer)
  {
    free(buffer);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  buffer = getcwd(buffer, PATH_MAX);
  if (!buffer)
  {
    free(buffer);
    perror("smash error: getcwd failed");
    return;
  }
  smash.setCurrDir(buffer);
  free(buffer);
}


//-----------------------------------------------Jobs-----------------------------------------------

JobsList::JobEntry::JobEntry(int jobId, pid_t pid, const char *cmd, bool isStopped) :
  m_jobId(jobId),
  m_pid(pid),
  m_isStopped(isStopped)
{
  strcpy(m_commandLine, cmd);
}

void JobsList::addJob(const char *cmd, pid_t pid, bool isStopped)
{
  removeFinishedJobs();
  int jobId = m_jobIdCounter + 1;
  m_jobIdCounter++;
  JobEntry newJob(jobId, pid, cmd, isStopped);
  m_list.push_back(newJob);
}

void JobsList::printJobsList(){
  removeFinishedJobs();
  for(const auto& job : m_list)
  {
    std::cout << "[" << job.m_jobId << "] " << job.m_commandLine << std::endl;
  }
}

void JobsList::removeFinishedJobs(){
  if (m_list.empty())
  {
    m_jobIdCounter = 0;
    return;
  }
  int maxActiveJobId = 0;
  for (auto it = m_list.begin(); it != m_list.end(); ++it)
  {
    auto job = *it;
    int exitStatus;
    int waitResult = waitpid(job.m_pid, &exitStatus, WNOHANG);
    if (waitResult == job.m_pid || waitResult == -1)
    {
      m_list.erase(it);
      --it;
    }
    else if (job.m_jobId > maxActiveJobId)
    {
      maxActiveJobId = job.m_jobId;
    }
  }
  m_jobIdCounter = maxActiveJobId;
}

int JobsList::getMaxId()
{
  return m_jobIdCounter;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
  for (auto it = m_list.begin(); it != m_list.end(); ++it) {
    if (it->m_jobId == jobId) {
      return &(*it);  // Return the address of the element in the vector
    }
  }
  return nullptr;
}

bool JobsList::isEmpty() const
{
  return m_list.empty();
}

void JobsList::removeJobById(int jobId){
  for (auto it = m_list.begin(); it != m_list.end(); ++it)
  {
    auto job = *it;
    if (job.m_jobId == jobId)
    {
      m_list.erase(it);
      return;
    }
  }
}

void JobsList::killAllJobs(){
  removeFinishedJobs();
  cout << "smash: sending SIGKILL signal to " << m_list.size() <<" jobs:" << endl;

  for (auto it = m_list.begin(); it != m_list.end(); ++it)
  {
    auto job = *it;
    cout << job.m_jobId << ": " << job.m_commandLine << endl;
    if (kill(job.m_pid, SIGKILL) == -1)
    {
      perror("smash error: kill failed");
    }
  }
}

//-----------------------------------------------BuiltInCommand-----------------------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command::Command(cmd_line) {}


//-------------------------------------ChangePromptCommand-------------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ChangePromptCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);
  SmallShell &smash = SmallShell::getInstance();
  if (argc == 1)
  {
    smash.changePrompt();
  }
  else
  {
    smash.changePrompt(string(args[1]));
  }
  deleteArguments(args);
}


//-------------------------------------ShowPidCommand-------------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_shellPid << endl;
}

//-------------------------------------GetCurrDirCommand-------------------------------------

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    initialCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

//-------------------------------------ChangeDirCommand-------------------------------------

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), m_prevDirectory(plastPwd) {}

void ChangeDirCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();

    // Initialize current directory if not set
    if (!strcmp(smash.getCurrDir(), "")) {
        initialCurrDir();
    }

    int argc = 0;
    char **argv = extractArguments(this->m_cmd_line, &argc);

    // Handle too many arguments
    if (argc > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
        deleteArguments(argv);
        return;
    }

    // Handle "cd -" when OLDPWD is not set
    if (argc == 2 && string(argv[1]) == "-" && !strcmp(*m_prevDirectory, "")) {
        cerr << "smash error: cd: OLDPWD not set" << endl;
        deleteArguments(argv);
        return;
    }

    // Handle "cd -" to switch to the previous directory
    if (argc == 2 && string(argv[1]) == "-") {
        if (chdir(*m_prevDirectory) == -1) {
            perror("smash error: chdir failed");
            deleteArguments(argv);
            return;
        }
        // Update directories
        char temp[PATH_MAX + 1];
        strcpy(temp, smash.getPrevDir());
        smash.setPrevDir();
        if (getcwd(smash.getCurrDir(), PATH_MAX) == nullptr) {
            perror("smash error: getcwd failed");
        }
        deleteArguments(argv);
        return;
    }

    // Change directory to the given path
    if (chdir(argv[1]) == -1) {
        perror("smash error: chdir failed");
        deleteArguments(argv);
        return;
    }

    // Update current and previous directories
    smash.setPrevDir();
    if (getcwd(smash.getCurrDir(), PATH_MAX) == nullptr) {
        perror("smash error: getcwd failed");
    }

    deleteArguments(argv);
}



//-------------------------------------JobsCommand-------------------------------------

JobsCommand::JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void JobsCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  smash.getAllJobs()->printJobsList();
}

//-------------------------------------Foreground-------------------------------------

bool isNumber(const char *str)
{
  for (int i = 0; str[i] != '\0'; i++)
  {
    if (!isdigit(str[i]))
    {
      return false;
    }
  }
  return true;
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList *jobs):
  BuiltInCommand(cmd_line) {
      m_jobs = jobs; // Initialize m_jobs in the constructor
  }

void ForegroundCommand::execute(){
  int argc;
  char **argv = extractArguments(this->m_cmd_line, &argc);

  int jobId;

  if (argc > 2 || (argc > 1 && !isNumber(argv[1])))
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    deleteArguments(argv);
    return;
  }

  if (argc == 1)
  {
    if (m_jobs->isEmpty())
    {
      cerr << "smash error: fg: jobs list is empty" << endl;
      deleteArguments(argv);
      return;
    }
    jobId = m_jobs->getMaxId();
  }
  else
  {
    jobId = stoi(argv[1]);
  }

  JobsList::JobEntry *job = m_jobs->getJobById(jobId);
  if (!job)
  {
    cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
    deleteArguments(argv);
    return;
  }

  SmallShell &smash = SmallShell::getInstance();
  if (job)
  {
    int jobPid = job->m_pid;
    if (job->m_isStopped)
    {
      if (kill(jobPid, SIGCONT) == -1)
      {
        perror("smash error: kill failed");
        deleteArguments(argv);
        return;
      }
    }

    int exitStatus;
    
    cout << job->m_commandLine << " " << jobPid << endl;

    smash.m_foregroundPid = jobPid;
    m_jobs->removeJobById(jobId);
    if (waitpid(jobPid, &exitStatus, WUNTRACED) == -1)
    {
      perror("smash error: waitpid failed");
      deleteArguments(argv);
      return;
    }
    smash.m_foregroundPid = 0;
  }
  deleteArguments(argv);
}

//-------------------------------------QuitCommand-------------------------------------

QuitCommand::QuitCommand(const char* cmd_line, JobsList *jobs):
 BuiltInCommand(cmd_line), m_jobs(jobs) {}

 void QuitCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);  

  if (argc > 1 && strcmp(args[1], "kill") == 0) {
    m_jobs->killAllJobs();
  }

  deleteArguments(args);
  exit(0);
}


//-------------------------------------KillCommand-------------------------------------

KillCommand::KillCommand(const char* cmd_line, JobsList *jobs):
 BuiltInCommand(cmd_line), m_jobs(jobs) {}

void KillCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);

  // Support signal with - prefix
  if (argc != 3 || args[1][0] != '-' || !isNumber(args[1] + 1) || !isNumber(args[2])) {
    cerr << "smash error: kill: invalid arguments" << endl;
    deleteArguments(args);
    return;
  }

  int signalNum = stoi(args[1] + 1);  // Skip the '-' character
  int jobId = stoi(args[2]);

  JobsList::JobEntry *job = m_jobs->getJobById(jobId);
  if (!job) {
    cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
    deleteArguments(args);
    return;
  }

  if (kill(job->m_pid, signalNum) == -1) {
    perror("smash error: kill failed");
    deleteArguments(args);
    return;
  }

  cout << "signal number " << signalNum << " was sent to pid " << job->m_pid << endl;

  deleteArguments(args);
}

//-------------------------------------AliasCommand-------------------------------------

AliasCommand::AliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

const std::regex AliasCommand::aliasPattern(R"(^alias [a-zA-Z0-9_]+='[^']*'$)");

void AliasCommand::printAllAliases() {
  SmallShell::getInstance().printAliases();
}

bool AliasCommand::checkAliasName(const std::string& aliasName) const {
  // Returns true if alias name is valid (not reserved and not taken)
  if (SmallShell::RESERVED_COMMANDS.count(aliasName)) {
    return false;
  }
  return !SmallShell::getInstance().isAliasNameTaken(aliasName);
}

void AliasCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);

  if (argc == 1) {
    printAllAliases();
    deleteArguments(args);
    return;
  }

  if (!std::regex_match(this->m_cmd_line, aliasPattern)) {
    cerr << "smash error: alias: invalid alias format" << endl;
    deleteArguments(args);
    return;
  }

  std::string input = _trim(std::string(this->m_cmd_line));
  size_t alias_start = input.find(' ') + 1; // Skip "alias "
  size_t equal_pos = input.find('=');
  
  // Extract alias name
  std::string alias_name = input.substr(alias_start, equal_pos - alias_start);
  
  // Extract command (with quotes)
  std::string commandWithQuotes = input.substr(equal_pos + 1);
  
  // Remove surrounding single quotes
  std::string command = commandWithQuotes.substr(1, commandWithQuotes.length() - 2);

  // Check if alias name is valid and not taken
  if (!checkAliasName(alias_name)) {
    cerr << "smash error: alias " << alias_name << " already exists or is a reserved command" << endl;
    deleteArguments(args);
    return;
  }

  // Add alias to the list in SmallShell
  SmallShell::getInstance().addAlias(alias_name, command);
  deleteArguments(args);
  return;
}


//-------------------------------------UnAliasCommand-------------------------------------
UnAliasCommand::UnAliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void UnAliasCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);
  SmallShell &smash = SmallShell::getInstance();
  
  if (argc == 1) {
    cerr << "smash error: unalias: not enough arguments" << endl;
    deleteArguments(args);
    return;
  }

  for (int i = 1; i < argc; ++i) {
    if (!smash.isAliasNameTaken(args[i])) {
      cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
      deleteArguments(args);
      return;
    }
    smash.removeAlias(args[i]);
  }
  deleteArguments(args);
}

//-------------------------------------UnSetEnvCommand-------------------------------------
UnSetEnvCommand::UnSetEnvCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

extern char **environ;

void UnSetEnvCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);

  if (argc == 1) {
    cerr << "smash error: unsetenv: not enough arguments" << endl;
    deleteArguments(args);
    return;
  }

  for (int i = 1; i < argc; ++i) {
    const char* var_name = args[i];
    size_t name_len = strlen(var_name);

    bool found = false;
    for (int j = 0; environ[j]; ++j) {
      if (strncmp(environ[j], var_name, name_len) == 0 && environ[j][name_len] == '=') {
        // Shift the remaining pointers left
        while (environ[j]) {
          environ[j] = environ[j + 1];
          ++j;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      cerr << "smash error: unsetenv: " << var_name << " does not exist" << endl;
    }
  }
  deleteArguments(args);
}

//-------------------------------------WatchProcCommand-------------------------------------
WatchProcCommand::WatchProcCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

std::string WatchProcCommand::readProcFile(pid_t pid, const std::string& fileName) {
  std::string path = "/proc/" + std::to_string(pid) + "/" + fileName;
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
      throw std::runtime_error("cannot open");
  }
  const size_t BUF_SIZE = 1024;
  std::string content;
  char buffer[BUF_SIZE];
  ssize_t bytesRead;
  while ((bytesRead = read(fd, buffer, BUF_SIZE)) > 0) {
      content.append(buffer, bytesRead);
  }
  close(fd);
  return content;
}

double WatchProcCommand::parseMemoryMb(const std::string& statusContent) {
  std::istringstream iss(statusContent);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      std::istringstream lineIss(line.substr(6));
      double kb = 0;
      lineIss >> kb;
      return kb / 1024.0;
    }
  }
  return 0.0;
}

double WatchProcCommand::parseCpuPercent(const std::string& statContent) {
    std::istringstream iss(statContent);
    std::vector<std::string> fields((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());
    // We need at least 22 fields: utime (14), stime (15), starttime (22)
    if (fields.size() < 22) {
        return 0.0;
    }
    long utime      = std::stol(fields[13]);
    long stime      = std::stol(fields[14]);
    long starttime  = std::stol(fields[21]);
    long clkTck     = sysconf(_SC_CLK_TCK);
    // Read system uptime via syscall
    int fd_uptime = open("/proc/uptime", O_RDONLY);
    if (fd_uptime == -1) {
        return 0.0;
    }
    const size_t UP_BUF_SIZE = 128;
    char upbuf[UP_BUF_SIZE];
    ssize_t upBytes = read(fd_uptime, upbuf, UP_BUF_SIZE - 1);
    close(fd_uptime);
    if (upBytes <= 0) {
        return 0.0;
    }
    upbuf[upBytes] = '\0';
    double uptimeSeconds = std::stod(std::string(upbuf));
    // Calculate CPU usage percentage
    double totalTime = (utime + stime) / static_cast<double>(clkTck);
    double seconds   = uptimeSeconds - (starttime / static_cast<double>(clkTck));
    if (seconds <= 0.0) {
        return 0.0;
    }
    return 100.0 * (totalTime / seconds);
}

void WatchProcCommand::execute() {
  int argc = 0;
  char **args = extractArguments(this->m_cmd_line, &argc);
  if (argc != 2 || !isNumber(args[1])) {
    cerr << "smash error: watchproc: invalid arguments" << endl;
    deleteArguments(args);
    return;
  }
  pid_t pid = static_cast<pid_t>(atoi(args[1]));
  deleteArguments(args);

  try {
    std::string status = WatchProcCommand::readProcFile(pid, "status");
    std::string stat   = WatchProcCommand::readProcFile(pid, "stat");
    double memMb = WatchProcCommand::parseMemoryMb(status);
    double cpuPct = WatchProcCommand::parseCpuPercent(stat);
    printf("PID: %d | CPU Usage: %.1f%% | Memory Usage: %.1f MB\n", pid, cpuPct, memMb);
  } catch(const std::exception&) {
    cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
  }
}


//-------------------------------------ExternalCommand-------------------------------------

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    // Trim the command line to remove leading and trailing whitespace
    string trimmedCmd = _trim(string(this->m_cmd_line));

    // Check if the command is empty after trimming
    if (trimmedCmd.empty()) {
        // Do nothing and return for empty or whitespace-only commands
        return;
    }

    // Check for complex commands (e.g., containing '*' or '?')
    bool isComplex = trimmedCmd.find("*") != string::npos || trimmedCmd.find("?") != string::npos;

    if (isComplex) {
        char trimmedCommand[COMMAND_MAX_ARGS];
        strcpy(trimmedCommand, trimmedCmd.c_str());

        char bashOption[] = "-c";
        char bashPath[] = "/bin/bash";
        char *bashArgs[] = {bashPath, bashOption, trimmedCommand, nullptr};

        if (execv(bashPath, bashArgs) == -1) {
            perror("smash error: execv failed");
            exit(0);
        }
    } else {
        int argc = 0;
        char **args = extractArguments(trimmedCmd.c_str(), &argc);

        if (argc == 0) {
            // If no arguments are parsed, return without doing anything
            deleteArguments(args);
            return;
        }

        string executable = string(args[0]);

        if (execvp(executable.c_str(), args) == -1) {
            perror("smash error: execvp failed");
            deleteArguments(args);
            return; // Do not exit the shell
        }

        deleteArguments(args);
    }
}


//-------------------------------------Special Commands-------------------------------------
//-------------------------------------Redirection Command-------------------------------------

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();

    // Copy the command line to safely modify it
    char cmd[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd, this->m_cmd_line);

    // Check for redirection operators
    char *overwrite = strstr(cmd, ">");
    char *append = strstr(cmd, ">>");

    if (overwrite != nullptr || append != nullptr) {
        // Determine the type of redirection
        bool isAppend = (append != nullptr);
        char *redirectOperator = isAppend ? append : overwrite;

        // Split the command and file path
        *redirectOperator = '\0'; // Terminate the command before the operator
        redirectOperator += isAppend ? 2 : 1; // Move past the operator
        std::string trimmedFilePath = _trim(std::string(redirectOperator));
        const char *filePath = trimmedFilePath.c_str();

        // Save the original STDOUT
        int originalStdout = dup(STDOUT_FILENO);
        if (originalStdout == -1) {
            perror("smash error: dup failed");
            return;
        }

        // Redirect output to the specified file
        int fd = open(filePath, O_WRONLY | O_CREAT | (isAppend ? O_APPEND : O_TRUNC), 0666);
        if (fd == -1) {
            perror("smash error: open failed");
            return;
        }

        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("smash error: dup2 failed");
            close(fd);
            return;
        }

        close(fd);

        // Execute the command
        smash.executeCommand(_trim(std::string(cmd)).c_str());

        // Restore the original STDOUT
        if (dup2(originalStdout, STDOUT_FILENO) == -1) {
            perror("smash error: dup2 restore failed");
        }
        close(originalStdout);
    }
}

//--------------------------------------------------------Pipe----------------------------------------------------------

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute() {
    // Enhanced parsing for | and |& pipe types
    string fullCmd = this->m_cmd_line;
    bool stderrPipe = false;
    size_t pipeIndex = fullCmd.find("|&");
    if (pipeIndex != string::npos) {
        stderrPipe = true;
    } else {
        pipeIndex = fullCmd.find('|');
    }
    string first = fullCmd.substr(0, pipeIndex);
    string sec   = fullCmd.substr(pipeIndex + (stderrPipe ? 2 : 1));

    int my_pipe[2];
    if (pipe(my_pipe) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) { // First child process
        if (setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            exit(1);
        }
        if (dup2(my_pipe[1], stderrPipe ? STDERR_FILENO : STDOUT_FILENO) == -1) {
            perror("smash error: dup2 failed");
            exit(1);
        }
        close(my_pipe[0]);
        close(my_pipe[1]);

        char *args1[COMMAND_MAX_ARGS];
        _parseCommandLine(first.c_str(), args1); // Parse arguments
        if (execvp(args1[0], args1) == -1) {
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    pid_t pid2 = fork();
    if (pid2 == 0) { // Second child process
        if (setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            exit(1);
        }
        if (dup2(my_pipe[0], STDIN_FILENO) == -1) {
            perror("smash error: dup2 failed");
            exit(1);
        }
        close(my_pipe[0]);
        close(my_pipe[1]);

        char *args2[COMMAND_MAX_ARGS];
        _parseCommandLine(sec.c_str(), args2); // Parse arguments
        if (execvp(args2[0], args2) == -1) {
            perror("smash error: execvp failed");
            exit(1);
        }
    }

    // Parent process
    close(my_pipe[0]);
    close(my_pipe[1]);

    int status;
    if (waitpid(pid1, &status, 0) == -1) {
        perror("smash error: waitpid failed");
    }
    if (waitpid(pid2, &status, 0) == -1) {
        perror("smash error: waitpid failed");
    }
}



//-------------------------------------DiskUsageCommand-------------------------------------

DiskUsageCommand::DiskUsageCommand(const char* cmd_line) : Command(cmd_line) {}

void DiskUsageCommand::execute() {
    // Parse arguments
    int argc = 0;
    char **args = extractArguments(this->m_cmd_line, &argc);
    // Too many arguments?
    if (argc > 2) {
        cerr << "smash error: du: too many arguments" << endl;
        deleteArguments(args);
        return;
    }
    // Determine target directory
    string dirPath;
    if (argc == 1) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            perror("smash error: getcwd failed");
            deleteArguments(args);
            return;
        }
        dirPath = string(cwd);
    } else {
        dirPath = string(args[1]);
    }
    deleteArguments(args);
    // Verify directory exists
    struct stat sb;
    if (stat(dirPath.c_str(), &sb) == -1 || !S_ISDIR(sb.st_mode)) {
        cerr << "smash error: du: directory " << dirPath << " does not exist" << endl;
        return;
    }
    // Traverse recursively and sum file sizes
    uint64_t totalBytes = 0;
    std::vector<std::string> stack;
    stack.push_back(dirPath);
    while (!stack.empty()) {
        std::string current = stack.back();
        stack.pop_back();
        DIR *dp = opendir(current.c_str());
        if (!dp) continue;
        struct dirent *entry;
        while ((entry = readdir(dp))) {
            std::string name = entry->d_name;
            // Skip "." and ".." directories to avoid infinite loop - cool!
            if (name == "." || name == "..") continue;
            std::string path = current + "/" + name;
            struct stat st;
            if (stat(path.c_str(), &st) == -1) continue;
            if (S_ISDIR(st.st_mode)) {
                stack.push_back(path);
            } else {
                totalBytes += st.st_size;
            }
        }
        closedir(dp);
    }
    uint64_t totalKB = (totalBytes + 1023) / 1024;
    cout << "Total disk usage: " << totalKB << " KB" << endl;
}



//-------------------------------------WhoAmICommand-------------------------------------

WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line) {}

void WhoAmICommand::execute() {
    uid_t uid = getuid();
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1) {
        perror("smash error: whoami: open failed");
        return;
    }
    const size_t BUF_SIZE = 1024;
    std::string partial;
    char buffer[BUF_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, BUF_SIZE)) > 0) {
        partial.append(buffer, bytesRead);
        size_t newlinePos;
        while ((newlinePos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, newlinePos);
            partial.erase(0, newlinePos + 1);
            std::vector<std::string> fields;
            std::istringstream iss(line);
            std::string field;
            while (std::getline(iss, field, ':')) {
                fields.push_back(field);
            }
            if (fields.size() >= 6 && static_cast<uid_t>(std::stoi(fields[2])) == uid) {
                std::cout << fields[0] << " " << fields[5] << std::endl;
                close(fd);
                return;
            }
        }
    }
    close(fd);
    std::cerr << "smash error: whoami: user id " << uid << " not found" << std::endl;
}

//-------------------------------------NetInfo-------------------------------------

NetInfo::NetInfo(const char* cmd_line) : Command(cmd_line) {}

void NetInfo::execute() {
    int argc = 0;
    char **args = extractArguments(this->m_cmd_line, &argc);
    if (argc < 2) {
        std::cerr << "smash error: netinfo: interface not specified" << std::endl;
        deleteArguments(args);
        return;
    }
    std::string iface = args[1];
    deleteArguments(args);

    // Check interface existence via ioctl
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("smash error: netinfo: socket failed");
        return;
    }
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
        std::cerr << "smash error: netinfo: interface " << iface << " does not exist" << std::endl;
        close(sock);
        return;
    }

    // IP Address
    if (ioctl(sock, SIOCGIFADDR, &ifr) == -1) {
        perror("smash error: netinfo: SIOCGIFADDR failed");
        close(sock);
        return;
    }
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
    std::cout << "IP Address: " << ip << std::endl;

    // Subnet Mask
    if (ioctl(sock, SIOCGIFNETMASK, &ifr) == -1) {
        perror("smash error: netinfo: SIOCGIFNETMASK failed");
        close(sock);
        return;
    }
    char mask[INET_ADDRSTRLEN];
    struct sockaddr_in *nm = (struct sockaddr_in *)&ifr.ifr_netmask;
    inet_ntop(AF_INET, &nm->sin_addr, mask, sizeof(mask));
    std::cout << "Subnet Mask: " << mask << std::endl;

    close(sock);

    // Default Gateway from /proc/net/route
    int fd = open("/proc/net/route", O_RDONLY);
    if (fd == -1) {
        perror("smash error: netinfo: open route failed");
        return;
    }
    std::string content;
    char buf[1024];
    ssize_t len;
    while ((len = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, len);
    }
    close(fd);
    std::istringstream iss(content);
    std::string line, gw;
    std::getline(iss, line); // skip header
    std::string gateway = "0.0.0.0";
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        std::string ifn, dst, hexgw;
        ls >> ifn >> dst >> hexgw;
        if (ifn == iface && dst == "00000000") {
            unsigned long g = strtoul(hexgw.c_str(), nullptr, 16);
            struct in_addr a; a.s_addr = g;
            char gwstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &a, gwstr, sizeof(gwstr));
            gateway = gwstr;
            break;
        }
    }
    std::cout << "Default Gateway: " << gateway << std::endl;

    // DNS Servers from /etc/resolv.conf
    fd = open("/etc/resolv.conf", O_RDONLY);
    if (fd == -1) {
        perror("smash error: netinfo: open resolv failed");
        return;
    }
    content.clear();
    while ((len = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, len);
    }
    close(fd);
    std::istringstream riss(content);
    std::vector<std::string> dns;
    while (std::getline(riss, line)) {
        std::istringstream ls2(line);
        std::string key, val;
        ls2 >> key >> val;
        if (key == "nameserver") {
            dns.push_back(val);
        }
    }
    std::cout << "DNS Servers: ";
    for (size_t i = 0; i < dns.size(); ++i) {
        std::cout << dns[i] << (i + 1 < dns.size() ? ", " : "");
    }
    std::cout << std::endl;
}

//-------------------------------------SmallShell-------------------------------------

SmallShell::SmallShell(): m_prompt("smash") {
  m_prevDir = (char *)malloc((PATH_MAX + 1) * sizeof(char));
  if (m_prevDir == nullptr)
  {
    free(m_prevDir);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  m_currDir = (char *)malloc((PATH_MAX + 1) * sizeof(char));
  strcpy(m_currDir, "");
  if (m_currDir == nullptr)
  {
    free(m_currDir);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(m_currDir, "");
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

pid_t SmallShell::m_shellPid = getpid();

const std::unordered_set<std::string> SmallShell::RESERVED_COMMANDS = {
  "chprompt",
  "showpid",
  "pwd",
  "cd",
  "jobs",
  "fg",
  "kill",
  "quit",
  "alias",
  "unalias",
  "unsetenv",
  "watchproc",
  "du"
};

std::string SmallShell::getPrompt() const
{
  return m_prompt;
}

void SmallShell::changePrompt(const std::string& new_prompt) {
  m_prompt = new_prompt;
}

Command *SmallShell::CreateCommand(const char *cmd_line) {
    // Track original user input for alias-background jobs
    static int __aliasDepth = 0;
    static std::string __originalCmd;
    __aliasDepth++;
    // On the first call, store the raw trimmed command
    if (__aliasDepth == 1) {
        __originalCmd = _trim(std::string(cmd_line));
    }

    // Ignore empty input
    std::string raw(cmd_line);
    if (raw.find_first_not_of(WHITESPACE) == std::string::npos) {
        __aliasDepth = 0;
        __originalCmd.clear();
        return nullptr;
    }

    // Detect background flag
    bool isBackground = _isBackgroundComamnd(raw.c_str());
    // Prepare trimmed command without trailing '&'
    std::string cmdTrim = _trim(raw);
    std::string noBg = cmdTrim;
    if (isBackground) {
        size_t pos = noBg.find_last_not_of(WHITESPACE);
        if (pos != std::string::npos && noBg[pos] == '&') {
            noBg = _trim(noBg.substr(0, pos));
        }
    }

    // Alias expansion: first word lookup, recursive re-dispatch
    size_t split = noBg.find_first_of(" \n");
    std::string first = (split == std::string::npos ? noBg : noBg.substr(0, split));
    std::string aliased;
    if (getAliasCommand(first, aliased)) {
        std::string newCmd = aliased + (isBackground ? " &" : "");
        Command* result = CreateCommand(newCmd.c_str());
        // __aliasDepth and __originalCmd managed by the deepest call
        return result;
    }

    // I/O Redirection
    if (noBg.find(">") != std::string::npos) {
        __aliasDepth = 0;
        __originalCmd.clear();
        return new RedirectionCommand(raw.c_str());
    }
    // Pipe
    if (noBg.find("|") != std::string::npos) {
        __aliasDepth = 0;
        __originalCmd.clear();
        return new PipeCommand(raw.c_str());
    }

    // Built-in commands
    if (first == "pwd")       { __aliasDepth = 0; __originalCmd.clear(); return new GetCurrDirCommand(raw.c_str()); }
    else if (first == "showpid")  { __aliasDepth = 0; __originalCmd.clear(); return new ShowPidCommand(raw.c_str()); }
    else if (first == "chprompt") { __aliasDepth = 0; __originalCmd.clear(); return new ChangePromptCommand(raw.c_str()); }
    else if (first == "cd")       { __aliasDepth = 0; __originalCmd.clear(); return new ChangeDirCommand(raw.c_str(), &m_prevDir); }
    else if (first == "jobs")     { __aliasDepth = 0; __originalCmd.clear(); return new JobsCommand(raw.c_str()); }
    else if (first == "fg")       { __aliasDepth = 0; __originalCmd.clear(); return new ForegroundCommand(raw.c_str(), &jobs); }
    else if (first == "kill")     { __aliasDepth = 0; __originalCmd.clear(); return new KillCommand(raw.c_str(), &jobs); }
    else if (first == "quit")     { __aliasDepth = 0; __originalCmd.clear(); return new QuitCommand(raw.c_str(), &jobs); }
    else if (first == "alias")    { __aliasDepth = 0; __originalCmd.clear(); return new AliasCommand(raw.c_str()); }
    else if (first == "unalias")  { __aliasDepth = 0; __originalCmd.clear(); return new UnAliasCommand(raw.c_str()); }
    else if (first == "unsetenv") { __aliasDepth = 0; __originalCmd.clear(); return new UnSetEnvCommand(raw.c_str()); }
    else if (first == "watchproc"){ __aliasDepth = 0; __originalCmd.clear(); return new WatchProcCommand(raw.c_str()); }
    else if (first == "du")       { __aliasDepth = 0; __originalCmd.clear(); return new DiskUsageCommand(raw.c_str()); }
    else if (first == "whoami")   { __aliasDepth = 0; __originalCmd.clear(); return new WhoAmICommand(raw.c_str()); }
    else if (first == "netinfo")  { __aliasDepth = 0; __originalCmd.clear(); return new NetInfo(raw.c_str()); }

    // External command: spawn child, set process group, exec
    pid_t pid = fork();
    if (pid < 0) {
        perror("smash error: fork failed");
        __aliasDepth = 0;
        __originalCmd.clear();
        return nullptr;
    }
    if (pid == 0) {
        // Child
        if (setpgrp() == -1) {
            perror("smash error: setpgrp failed");
        }
        // exec in-place
        ExternalCommand ext(raw.c_str());
        ext.execute();
        exit(1);
    } else {
        // Parent
        if (!isBackground) {
          m_foregroundPid = pid;
          int status;
          waitpid(pid, &status, WUNTRACED);
          m_foregroundPid = 0;
        } else {
            // If alias-expanded background, show the original user input
            const char* jobCmd = (__aliasDepth > 1 ? __originalCmd.c_str() : cmdTrim.c_str());
            jobs.addJob(jobCmd, pid);
        }
        // Reset alias tracking after job registration
        __aliasDepth = 0;
        __originalCmd.clear();
        return nullptr;
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
  Command *cmd = CreateCommand(cmd_line);
  if (cmd == nullptr)
  {
    delete cmd;
    return;
  }
  cmd->execute();
  if (dynamic_cast<QuitCommand*>(cmd) != nullptr)
  {
    delete cmd;
    exit(0);
  }
  delete cmd;
}

char *SmallShell::getCurrDir() const {
return m_currDir;
}

void SmallShell::setCurrDir(const char *newDir)
{
strcpy(m_currDir, newDir);
}

char *SmallShell::getPrevDir() const
{
return m_prevDir;
}

void SmallShell::setPrevDir()
{
strcpy(m_prevDir, m_currDir);
}

JobsList *SmallShell::getAllJobs()
{
return &jobs;
}




//-------------------------------------SmallShell Aliases Implementation-------------------------------------

void SmallShell::addAlias(const std::string& name, const std::string& command) {
  // Overwrite if already exists
  for (auto& p : m_aliases) {
    if (p.first == name) {
      p.second = command;
      return;
    }
  }
  m_aliases.push_back(std::make_pair(name, command));
}

bool SmallShell::getAliasCommand(const std::string& name, std::string& outCommand) const {
  for (const auto& p : m_aliases) {
    if (p.first == name) {
      outCommand = p.second;
      return true;
    }
  }
  return false;
}

void SmallShell::printAliases() const {
  for (const auto& p : m_aliases) {
    std::cout << p.first << "='" << p.second << "'" << std::endl;
  }
}

bool SmallShell::isAliasNameTaken(const std::string& name) const {
  for (const auto& p : m_aliases) {
    if (p.first == name) {
      return true;
    }
  }
  return false;
}

void SmallShell::removeAlias(const std::string& name) {
  for (auto it = m_aliases.begin(); it != m_aliases.end(); ++it) {
    if (it->first == name) {
      m_aliases.erase(it);
      return;
    }
  }
}