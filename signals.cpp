#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell &shell = SmallShell::getInstance();
    pid_t fg = shell.m_foregroundPid; 
  
    if (fg) {
        if (kill(fg, SIGINT) == -1) {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << shell.m_foregroundPid << " was killed" << endl;
    }
    shell.m_foregroundPid = 0;
}