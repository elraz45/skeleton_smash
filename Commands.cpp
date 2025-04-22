#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

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

//-----------------------------------------------Command-----------------------------------------------

Command::Command(const char *cmd_line) : m_cmd_line(cmd_line) {}

Command::~Command()
{
  m_cmd_line = nullptr;
}


//-----------------------------------------------BuiltInCommand-----------------------------------------------

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command::Command(cmd_line) {}


//-------------------------------------ChangePromptCommand-------------------------------------

ChpromptCommand::ChpromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ChpromptCommand::execute() {
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






//-------------------------------------SmallShell-------------------------------------

//
SmallShell::SmallShell(): m_prompt("smash") {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

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
    return new ChpromptCommand(cmd_line);
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

void QuitCommand::execute() {
    // TODO: Add implementation
}

void RedirectionCommand::execute() {
    // TODO: Add implementation
}