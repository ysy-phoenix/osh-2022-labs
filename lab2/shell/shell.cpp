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
#include <sys/wait.h>

#include "shell.h"

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // 用来存储读入的一行命令
    std::string cmd;
    while (true) {
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

        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);
        cmd = Trim(cmd);

        // 按管道符拆解命令行
        std::vector<std::string> cmds = split(cmd, "|");

        if (cmds.size() == 0) {
            continue;
        } else if (cmds.size() == 1) { // 没有管道的单一命令
            // 按空格分割命令为单词
            std::vector<std::string> args = split(cmds[0], " ");

            execute(args);
        } else { // 多管道命令
            exec_pipe(cmds);
        }
    }
}
