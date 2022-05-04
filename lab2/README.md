## 实现的选做

新打开的 shell 能使用之前保留的记录。（历史命令存储在 `~/.shell_history` 下）

处理 Ctrl-D (EOF) 按键

`echo ~root`                         

`echo $SHELL`

## history

history 相关的实现基本与 bash 功能等同。

一些细节如下：

1.存入 .shell_history 的命令经过 trim，如删去不必要的空格。

2.连续多条相同命令只存一条，也就是 .shell_history 中不会有连续两条相同命令。

3.支持 history、!!、!n 与管道或重定向相结合。

## 其它

shell 提示符与 bash 风格类似。

strace 用法为 `./strace cmd`


