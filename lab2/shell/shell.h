#ifndef SHELL_H
#define SHELL_H

#define WRITE_END 1 // pipe write end
#define READ_END 0  // pipe read end

inline std::string &LeftTrim(std::string &s, const char *t = " \t\n\r\f\v");
inline std::string &RightTrim(std::string &s, const char *t = " \t\n\r\f\v");
inline std::string &Trim(std::string &s, const char *t = " \t\n\r\f\v");
std::vector<std::string> split(std::string s, const std::string &delimiter);
int exec_cd(std::vector<std::string> args);
int exec_pwd(std::vector<std::string> args);
int exec_export(std::vector<std::string> args);
int exec_exit(std::vector<std::string> args);
int exec_builtin(std::vector<std::string> args);
void execute(std::vector<std::string> args);
void exec_pipe(std::vector<std::string> args);

// trim from left
inline std::string &LeftTrim(std::string &s, const char *t) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string &RightTrim(std::string &s, const char *t) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string &Trim(std::string &s, const char *t) {
    return LeftTrim(RightTrim(s, t), t);
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    s = Trim(s);
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
        s = Trim(s);
    }
    res.push_back(s);
    return res;
}

int exec_cd(std::vector<std::string> args) {
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

int exec_pwd(std::vector<std::string> args) {
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

int exec_export(std::vector<std::string> args) {
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

int exec_exit(std::vector<std::string> args) {
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

int exec_builtin(std::vector<std::string> args) {
    // 没有可处理的命令
    if (args.empty()) {
        return 0;
    } else if (args[0] == "cd") {
        // 更改工作目录为目标目录
        exec_cd(args);
    } else if (args[0] == "pwd") {
        // 显示当前工作目录
        exec_pwd(args);
    } else if (args[0] == "export") {
        // 设置环境变量
        exec_export(args);
    } else if (args[0] == "exit") {
        // 退出
        exec_exit(args);
    } else {
        // 不是内置指令
        return -1;
    }
    return 0;
}

void execute(std::vector<std::string> args) {
    // 运行内置命令
    if (exec_builtin(args) == 0) {
        return;
    }

    // 外部命令
    pid_t pid = fork();

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    if (pid == 0) {
        // 这里只有子进程才会进入
        // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
        // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
        execvp(args[0].c_str(), arg_ptrs);

        // 所以这里直接报错
        exit(255);
    }

    // 这里只有父进程（原进程）才会进入
    int ret = wait(nullptr);
    if (ret < 0) {
        std::cout << "wait failed";
    }
}

void exec_pipe(std::vector<std::string> cmds) {
    int read_fd; // 上一个管道的读端口（出口）
    int cnt = cmds.size();
    for (int i = 0; i < cnt; i++) {
        int pipefd[2], ret, lastReadEnd;
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
        char *arg_ptrs[args.size() + 1];
        for (auto i = 0; i < args.size(); i++) {
            arg_ptrs[i] = &args[i][0];
        }
        arg_ptrs[args.size()] = nullptr;

        if (pid == 0) {
            // 除最后一条命令，都将标准输出重定向到当前管道写口
            if (i < cnt - 1) {
                dup2(pipefd[WRITE_END], STDOUT_FILENO);
            }
            // 除首条命令，都将标准输入重定向到上一个管道读口
            if (i > 0) {
                dup2(lastReadEnd, STDIN_FILENO);
            }

            if (exec_builtin(args) == 0) {
                continue;
            }
            execvp(args[0].c_str(), arg_ptrs);
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
#endif