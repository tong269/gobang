#include "socket_func.h"
#include <iostream>
#include <string>
#include <cstring>

#include "jsoncpp/json/json_features.h"
#include "jsoncpp/json/writer.h"


bool sendJsonMsg(const Json::Value& jsonMsg, SocketFD fd) {
    return sendJsonMsg(Json::FastWriter().write(jsonMsg), fd);
}


//向指定fd发送消息，消息格式     |length:xxxx|string|   length后面的字段表示string的长度，客户端拿到后先将string转为Json对象，然后读取消息
bool sendJsonMsg(const std::string& msg, SocketFD fd) {
    if (msg.empty())
        return false;

    int len = msg.length();
    std::string msgSend;
    msgSend.reserve(12 + len);
    msgSend += "length:";
    msgSend += std::to_string(len);

    //填充字节
    if (len < 10)
        msgSend += "   ";
    else if (len < 100)
        msgSend += "  ";
    else if (len < 1000)
        msgSend += " ";
    msgSend += "\n";
    msgSend += msg;

    std::cout << "Sending message\n" << msgSend << std::endl;

    send(fd, msgSend.c_str(), len + 12, 0);
    return true;
}


int recvJsonMsg(Json::Value& root, SocketFD fd) {
    char temp[8] = { 0 };
    char lengthStr[5] = { 0 };
    char msgLengthInfo[13] = { 0 };

    //接收信息,先接受12个字节 length:(7字节) + int(4字节) + '\n'(1字节)
    int n = recv(fd, msgLengthInfo, 12, 0);
    if (n <= 0)
        return -1;
    if (n < 12)
        return 0;
    //获取消息长度 （length:）
    memcpy(temp, msgLengthInfo, 7);
    memcpy(lengthStr, msgLengthInfo+7, 4);
    temp[7] = 0;
    if (strcmp(temp, "length:") != 0)
        return 0;

    int msgLength = 0;
    sscanf(lengthStr, "%d", &msgLength);

    char* jsonMsg = new char[msgLength+1];
    jsonMsg[msgLength] = 0;
    n = recv(fd, jsonMsg, msgLength, 0);
    
    bool ret = 0;

    do {
        if (n <= 0) {
            ret = -1;
            break;
        }
        if (n < msgLength) {
            break;
        }

        Json::Reader reader;
        //解析json消息到root里面去
        if (!reader.parse(jsonMsg, root))
            break;

        ret = 1;
    } while (false);

    if (ret == 1) {
        printf("Recieved message:\n%s\n", jsonMsg);
    }

    delete[] jsonMsg;
    return ret;
}

//关闭连接
void closeSocket(SocketFD fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close (fd);
#endif
}

