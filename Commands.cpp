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

// TODO: Add your implementation for classes in Commands.h 


//-----------------------------------------------Helper Functions-------------------------------------------------------
char **getArgs(const char *cmd_line, int *numArgs)
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
  *numArgs = _parseCommandLine(cmd_line_clean, args);
  return args;
}

void deleteArgs(char **args)
{
  // Delete each argument in args:
  for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++)
  {
    free(args[i]);
  }
  // Delete args itself:
  free(args);
}

bool checkFullPath(char *newPath)
{
  if (newPath[0] == '/')
    return true;
  return false;
}

char *goUp(char *dir)
{
  if (!strcmp(dir, "/"))
  {
    return dir;
  }
  int cut = string(dir).find_last_of("/");
  dir[cut] = '\0';
  return dir;
}


//-----------------------------------------------Command-----------------------------------------------

Command::Command(const char *cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command()
{
  m_cmd_line = nullptr;
}

void Command::firstUpdateCurrDir()
{
  SmallShell &smash = SmallShell::getInstance();
  char *buffer = (char *)malloc(PATH_MAX * sizeof(char) + 1);
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


//-----------------------------------------------BuiltInCommand-----------------------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command::Command(cmd_line) {}


//-------------------------------------ChangePromptCommand-------------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ChangePromptCommand::execute() {
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  SmallShell &smash = SmallShell::getInstance();
  if (numArgs == 1)
  {
    smash.chngPrompt();
  }
  else
  {
    smash.chngPrompt(string(args[1]));
  }
  deleteArgs(args);
}


//-------------------------------------ShowPidCommand-------------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  SmallShell &smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.m_pid << endl;
}

//-------------------------------------GetCurrDirCommand-------------------------------------

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    firstUpdateCurrDir();
  }
  cout << string(smash.getCurrDir()) << endl;
}

//-------------------------------------ChangeDirCommand-------------------------------------

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line),
                                                                            m_plastPwd(plastPwd) {}

void ChangeDirCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  if (!strcmp(smash.getCurrDir(), ""))
  {
    firstUpdateCurrDir();
  }
  int numArgs = 0;
  char **args = getArgs(this->m_cmd_line, &numArgs);
  if (numArgs > 2) // The command itself counts as an arg
  {
    cerr << "smash error: cd: too many arguments" << endl;
    deleteArgs(args);
    return;
  }
  else if (!strcmp(*m_plastPwd, "") && string(args[1]) == "-")
  {
    cerr << "smash error: cd: OLDPWD not set" << endl;
    deleteArgs(args);
    return;
  }
  else if (string(args[1]) == "-")
  {
    if (chdir(*m_plastPwd) == -1)
    {
      perror("smash error: chdir failed");
      deleteArgs(args);
      return;
    }
    // Switch current and previous directories
    char temp[PATH_MAX + 1];
    strcpy(temp, smash.getPrevDir());
    smash.setPrevDir();
    smash.setCurrDir(temp);
    deleteArgs(args);
    return;
  }
  if (chdir(args[1]) == -1)
  {
    perror("smash error: chdir failed");
    deleteArgs(args);
    return;
  }
  // If the given "path" is to go up, remove the last part of the current path
  if (string(args[1]) == "..")
  {
    smash.setPrevDir();
    goUp(smash.getCurrDir());
    deleteArgs(args);
    return;
  }

  // If the new path is the full path, set currDir equal to it
  if (checkFullPath(args[1]))
  {
    smash.setPrevDir();
    smash.setCurrDir(args[1]);
  }
  // If not, append the new folder to the end of the current path
  else
  {
    smash.setPrevDir();
    smash.setCurrDir(smash.getCurrDir(), args[1]);
  }
  deleteArgs(args);
}





void QuitCommand::execute() {
  // TODO: Add implementation
}

void RedirectionCommand::execute() {
  // TODO: Add implementation
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

pid_t SmallShell::m_pid = getpid();

std::string SmallShell::getPrompt() const
{
  return m_prompt;
}

void SmallShell::chngPrompt(const std::string& new_prompt) {
  m_prompt = new_prompt;
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
  /*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */

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
    


    return nullptr;
}


void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    // Command* cmd = CreateCommand(cmd_line);
    // cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)

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



char *SmallShell::getCurrDir() const
{
  return m_currDir;
}

void SmallShell::setCurrDir(char *currDir, char *toCombine)
{
  if (toCombine == nullptr)
  {
    strcpy(m_currDir, currDir);
    return;
  }
  int length = string(currDir).length() + string(toCombine).length() + 2;
  char *temp = (char *)malloc(length * sizeof(char));
  if (temp == nullptr)
  {
    free(temp);
    cerr << "smash error: malloc failed" << endl;
    return;
  }
  strcpy(temp, currDir);
  if (strcmp(currDir, "/") != 0)
  {
    strcat(temp, "/");
  }
  strcat(temp, toCombine);
  strcpy(m_currDir, temp);
  free(temp);
}

char *SmallShell::getPrevDir() const
{
  return m_prevDir;
}

void SmallShell::setPrevDir()
{
  strcpy(m_prevDir, m_currDir);
}





