#ifndef SHELL_H
#define SHELL_H

#define WRITE_END 1 // pipe write end
#define READ_END 0  // pipe read end

inline std::string &leftTrim(std::string &s, const char *t = " \t\n\r\f\v");
inline std::string &rightTrim(std::string &s, const char *t = " \t\n\r\f\v");
inline std::string &trim(std::string &s, const char *t = " \t\n\r\f\v");
std::vector<std::string> split(std::string s, const std::string &delimiter);
int exeCd(std::vector<std::string> args);
int exePwd(std::vector<std::string> args);
int exeExport(std::vector<std::string> args);
int exeExit(std::vector<std::string> args);
int exeBuiltinCmd(std::vector<std::string> args);
void exeSingleCmd(std::vector<std::string> args);
void exePipeCmd(std::vector<std::string> args);
void redirect(std::vector<std::string> &args);

// trim from left
inline std::string &leftTrim(std::string &s, const char *t) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string &rightTrim(std::string &s, const char *t) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string &trim(std::string &s, const char *t) {
    return leftTrim(rightTrim(s, t), t);
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    s = trim(s);
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
        s = trim(s);
    }
    res.push_back(s);
    return res;
}

int exeCd(std::vector<std::string> args) {
    if (args.size() <= 1) {
        // 输出的信息尽量为英文，非英文输出（其实是非 ASCII 输出）在没有特别配置的情况下（特别是 Windows
        // 下）会乱码 如感兴趣可以自行搜索 GBK Unicode UTF-8 Codepage UTF-16 等进行学习
        std::cout << "Insufficient arguments\n";
        // 不要用 std::endl，std::endl = "\n" + fflush(stdout)
        return -1;
    }

    // 调用系统 API
    int ret = chdir(args[1].c_str());
    if (ret < 0) {
        std::cout << "cd failed\n";
    }
    return 0;
}

int exePwd(std::vector<std::string> args) {
    std::string cwd;

    // 预先分配好空间
    cwd.resize(PATH_MAX);

    // std::string to char *: &s[0]（C++17 以上可以用 s.data()）
    // std::string 保证其内存是连续的
    const char *ret = getcwd(&cwd[0], PATH_MAX);
    if (ret == nullptr) {
        std::cout << "pwd failed\n";
    } else {
        std::cout << ret << "\n";
    }
    return 0;
}

int exeExport(std::vector<std::string> args) {
    for (auto i = ++args.begin(); i != args.end(); i++) {
        std::string key = *i;

        // std::string 默认为空
        std::string value;

        // std::string::npos = std::string end
        // std::string 不是 nullptr 结尾的，但确实会有一个结尾字符 npos
        size_t pos;
        if ((pos = i->find('=')) != std::string::npos) {
            key = i->substr(0, pos);
            value = i->substr(pos + 1);
        }

        int ret = setenv(key.c_str(), value.c_str(), 1);
        if (ret < 0) {
            std::cout << "export failed\n";
        }
    }
    return 0;
}

int exeExit(std::vector<std::string> args) {
    if (args.size() <= 1) {
        exit(0);
    }

    // std::string 转 int
    std::stringstream code_stream(args[1]);
    int code = 0;
    code_stream >> code;

    // 转换失败
    if (!code_stream.eof() || code_stream.fail()) {
        std::cout << "Invalid exit code\n";
        return 0;
    }

    exit(code);
    return 0;
}

int exeBuiltinCmd(std::vector<std::string> args) {
    // 没有可处理的命令
    if (args.empty()) {
        return 0;
    } else if (args[0] == "cd") {
        // 更改工作目录为目标目录
        exeCd(args);
    } else if (args[0] == "pwd") {
        // 显示当前工作目录
        exePwd(args);
    } else if (args[0] == "export") {
        // 设置环境变量
        exeExport(args);
    } else if (args[0] == "exit") {
        // 退出
        exeExit(args);
    } else {
        // 不是内置指令
        return -1;
    }
    return 0;
}

void exeSingleCmd(std::vector<std::string> args) {
    // 运行内置命令
    if (exeBuiltinCmd(args) == 0) {
        exit(0);
    }

    redirect(args);

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    execvp(args[0].c_str(), arg_ptrs);

    return;
}

void exePipeCmd(std::vector<std::string> cmds) {
    int lastReadEnd; // 上一个管道的读端口（出口）
    int cnt = cmds.size();

    for (int i = 0; i < cnt; i++) {
        int pipefd[2], ret;
        // 创建管道
        if (i < cnt - 1) {
            ret = pipe(pipefd);
        }

        if (ret < 0) {
            std::cout << "pipe error!\n";
            return;
        }

        pid_t pid = fork();

        std::vector<std::string> args = split(cmds[i], " ");

        if (pid == 0) {
            // 除最后一条命令，都将标准输出重定向到当前管道写口
            if (i < cnt - 1) {
                dup2(pipefd[WRITE_END], STDOUT_FILENO);
            }
            // 除首条命令，都将标准输入重定向到上一个管道读口
            if (i > 0) {
                dup2(lastReadEnd, STDIN_FILENO);
            }

            exeSingleCmd(args);
            exit(255);
        }
        if (i > 0) {
            close(lastReadEnd);
        }
        if (i < cnt - 1) {
            lastReadEnd = pipefd[READ_END];
        }
        close(pipefd[WRITE_END]);
    }

    // 等待所有子进程结束
    while (wait(nullptr) > 0)
        ;
}

void redirect(std::vector<std::string> &args) {
    /* 支持基本的文件重定向 */
    std::vector<std::string>::iterator it;
    if ((it = find(args.begin(), args.end(), ">")) != args.end()) {
        std::string file = *(it + 1);
        int fd = open(&file[0], O_RDWR);
        if (fd == -1) {
            std::cout << file << ": No such file or directory\n";
            exit(2);
        }
        dup2(fd, STDOUT_FILENO); //将标准输出重定向
        close(fd);
        *it = "";
        *(it + 1) = "";
    }
    if ((it = find(args.begin(), args.end(), ">>")) != args.end()) {
        std::string file = *(it + 1);
        int fd = open(&file[0], O_RDWR | O_APPEND);
        if (fd == -1) {
            std::cout << file << ": No such file or directory\n";
            exit(2);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        *it = "";
        *(it + 1) = "";
    }
    if ((it = find(args.begin(), args.end(), "<")) != args.end()) {
        std::string file = *(it + 1);
        int fd = open(&file[0], O_RDWR);
        if (fd == -1) {
            std::cout << file << ": No such file or directory\n";
            exit(2);
        }
        dup2(fd, STDIN_FILENO); // 将标准输入重定向
        close(fd);
        *it = "";
        *(it + 1) = "";
    }
    args.erase(remove(args.begin(), args.end(), ""), args.end());
} // end orient if

#endif