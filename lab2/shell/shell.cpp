// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <sys/wait.h>
// file

#include <fstream>
#include <iomanip>

#include "shell.h"

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // init
    int status;
    lastCmd = "";
    nthCmd = "";
    isLastCmd = false;
    isNthCmd = false;
    history.clear();

    std::string homePath = getenv("HOME");
    std::fstream historyFile(homePath + "/.shell_history", std::ios::app);
    if (!historyFile) {
        std::cout << "open history failed!" << std::endl;
        exit(1);
    }
    readHistory(history);

    while (true) {
        _pid = fork();
        if (_pid < 0) {
            std::cout << "fork failed\n";
        } else if (_pid == 0) { // 子进程
            signal(SIGINT, handler);
            // 用来存储读入的一行命令
            std::string cmd;

            while (true) {

                if (!isLastCmd && !isNthCmd) {
                    // 打印提示符
                    std::string cwd;

                    // 预先分配好空间
                    cwd.resize(PATH_MAX);

                    const char *path = getcwd(&cwd[0], PATH_MAX);
                    if (path == nullptr) {
                        exit(EXIT_FAILURE);
                    } else {
                        std::cout << "\e[1;44m"
                                  << "# " << path << " "
                                  << "\e[0m"
                                  << " ";
                    }
                }

                // cmd
                if (isLastCmd) {
                    cmd = lastCmd;
                    isLastCmd = false;
                } else if (isNthCmd) {
                    cmd = nthCmd;
                    isNthCmd = false;
                } else if (std::cin.peek() != EOF) {
                    // 读入一行。std::getline 结果不包含换行符。
                    std::getline(std::cin, cmd);
                    cmd = trim(cmd);
                } else {
                    std::cout << "exit" << std::endl;
                    exit(0);
                }

                if (cmd == "") {
                    continue;
                }

                // history
                if (cmd != lastCmd && cmd.substr(0, 1) != "!") {
                    historyFile << cmd << std::endl;
                    history.push_back(cmd);
                }

                // lastCmd
                if (cmd.substr(0, 1) != "!") {
                    if (lastCmd != "") {
                        lastCmd = cmd;
                    } else {
                        lastCmd = history.back();
                    }
                }

                // 按管道符拆解命令行
                std::vector<std::string> cmds = split(cmd, "|");

                if (cmds.size() == 0) {
                    continue;
                } else if (cmds.size() == 1) { // 没有管道的单一命令
                    // 按空格分割命令为单词
                    std::vector<std::string> args = split(cmds[0], " ");

                    // 内置命令
                    if (exeBuiltinCmd(args) == 0) {
                        continue;
                    }

                    // 外部命令
                    pid_t pid = fork();

                    if (pid == 0) {
                        // 这里只有子进程才会进入
                        // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
                        // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
                        exeSingleCmd(args);

                        // 所以这里直接报错
                        exit(255);
                    }

                    // 这里只有父进程（原进程）才会进入
                    int ret = wait(nullptr);
                    if (ret < 0) {
                        std::cout << "wait failed\n";
                    }
                } else { // 多管道命令
                    exePipeCmd(cmds);
                }
            } // end while
        } else {
            signal(SIGINT, handler);
            waitpid(_pid, &status, 0);
            if (status != INT) {
                historyFile.close();
                exit(status);
            } else {
                continue;
            }
        } // end if
    }     // end while
}
