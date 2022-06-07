# xthread

#### 介绍
一个用户级线程库，用法同pthread，使用内嵌汇编进行上下文保存，仅支持Linux x86_64环境下gcc编译

由于 x86_64 指令集下 System V abi 的二进制文件存在 red zone，而目前的实现中，线程的上下文保存在当前线程的栈中(push pop)，因此可能与函数的局部变量冲突。

参考：https://stackoverflow.com/questions/6380992/inline-assembly-that-clobbers-the-red-zone

目前没有很好的解决方法，请在编译时加上`-mno-red-zone`
