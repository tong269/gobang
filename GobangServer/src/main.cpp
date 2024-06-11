#include "gobangserver.h"

#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

GobangServer server;

void handleCtrlC(int num);
void exitFunc();
//程序入口
int main(int argc, char** argv) {
    //退出时执行exitfunc
    atexit(exitFunc);
    //处理ctrl + c 信号
    signal(SIGINT, handleCtrlC);

    srand(time(NULL));


    //启动服务器
    if (!server.start(6666)) {
        std::cerr << "Start failed!" << std::endl;
        return 1;
    }

    return 0;
}
//565
void handleCtrlC(int num) {
    exit(0);
}

void exitFunc() {
    server.stop();
}

