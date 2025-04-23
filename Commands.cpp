#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>


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

char **extractArguments(const char *cmd_line, int *argc)
{
  // Remove background sign if exists:
  char cmd[COMMAND_MAX_LENGTH];
  strcpy(cmd, cmd_line);
  _removeBackgroundSign(cmd);
  const char *cmd_line_clean = cmd;

  // Parse arguments:
  char **args = (char **)malloc((COMMAND_MAX_LENGTH + 1) * sizeof(char *));
  if (args == nullptr)
  {
    cerr << "smash error: malloc failed" << endl;
    free(args);
    return nullptr;
  }
  // Initialize args to avoid valgrind errors:
  std::fill_n(args, COMMAND_MAX_LENGTH + 1, nullptr);
  *argc = _parseCommandLine(cmd_line_clean, args);
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

Command::Command(const char *cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command()
{
  m_cmd_line = nullptr;
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
  strcpy(m_cmd, cmd);
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
    std::cout << "[" << job.m_jobId << "] " << job.m_cmd << job.m_pid << std::endl;
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





void QuitCommand::execute() {
  // TODO: Add implementation
}

void RedirectionCommand::execute() {
  // TODO: Add implementation
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
 BuiltInCommand(cmd_line), m_jobs(jobs) {}

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
    
    cout << job->m_cmd << " " << jobPid << endl;

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

std::string SmallShell::getPrompt() const
{
  return m_prompt;
}

void SmallShell::changePrompt(const std::string& new_prompt) {
  m_prompt = new_prompt;
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &m_prevDir);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line);
  }
  else if (firstWord.compare("fg") == 0) {
    return new ForegroundCommand(cmd_line, &jobs);
  }
    


    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {

    Command *cmd = CreateCommand(cmd_line);
    if (cmd == nullptr)
    {
      delete cmd;
      return;
    }
    cmd->execute();
    if (dynamic_cast<QuitCommand*>(cmd) != nullptr || dynamic_cast<RedirectionCommand *>(cmd) != nullptr)
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



