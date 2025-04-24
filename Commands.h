// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <unordered_set>
#include <string.h>
#include <regex>


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class JobsList;

class Command {
public:
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
protected:
    // command line
    char *m_cmd_line;

    void initialCurrDir();
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
        m_cmd_line = nullptr;
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    NetInfo(const char *cmd_line);

    virtual ~NetInfo() {
    }

    void execute() override;
};


class ChangeDirCommand : public BuiltInCommand {
    // TODO: Add your data members
    public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;

    private:
    char **m_prevDirectory;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
    public:
    ChangePromptCommand(const char *cmd_line);

        virtual ~ChangePromptCommand() {
        }

        void execute() override;
    };

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
private:
    JobsList *m_jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
private:
    JobsList *m_jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        public:
        JobEntry(int jobId, pid_t pid, const char *cmd, bool isStopped = false);
        ~JobEntry(){}
        int m_jobId;
        pid_t m_pid;
        char m_commandLine[COMMAND_MAX_LENGTH + 1];
        bool m_isStopped;
    };

    // TODO: Add your data members
    JobsList() = default;

    ~JobsList() = default;

    void addJob(const char *cmd, pid_t pid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    int getMaxId();

    bool isEmpty() const;

    // TODO: Add extra methods or modify exisitng ones as needed

    private:
    std::vector<JobEntry> m_list;
    int m_jobIdCounter = 0;
};

class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    JobsCommand(const char *cmd_line);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList *m_jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
private:
    bool checkAliasName(const std::string& aliasName) const;
    void printAllAliases();

public:
    static const std::regex aliasPattern;

    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class WatchProcCommand : public BuiltInCommand {
public:
    WatchProcCommand(const char *cmd_line);

    virtual ~WatchProcCommand() {
    }

    void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members
    SmallShell();
    std::string m_prompt;
    char *m_currDir;
    char *m_prevDir;
    JobsList jobs;

public:
    static pid_t m_shellPid;
    int m_foregroundPid;
    static const std::unordered_set<std::string> RESERVED_COMMANDS;

    // Aliases
    std::vector<std::pair<std::string, std::string>> m_aliases;
    void addAlias(const std::string& name, const std::string& command);
    bool getAliasCommand(const std::string& name, std::string& outCommand) const;
    void printAliases() const;
    bool isAliasNameTaken(const std::string& name) const;
    void removeAlias(const std::string& name);
    
    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    Command *CreateCommand(const char *cmd_line);

    void executeCommand(const char *cmd_line);

    std::string getPrompt() const;

    void changePrompt(const std::string& new_prompt = "smash");

    void setCurrDir(const char *newDir);

    char *getCurrDir() const;

    void setPrevDir();

    char *getPrevDir() const;

    JobsList *getAllJobs();

};

#endif //SMASH_COMMAND_H_
