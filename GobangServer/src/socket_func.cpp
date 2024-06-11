#include "socket_func.h"
#include <iostream>
#include <string>
#include <cstring>

#include "jsoncpp/json/json_features.h"
#include "jsoncpp/json/writer.h"

/**
 * 发送JSON消息
 * @param jsonMsg 要发送的JSON消息
 * @param fd 目标套接字文件描述符
 * @return 发送是否成功
 */
bool sendJsonMsg(const Json::Value& jsonMsg, SocketFD fd) {
    return sendJsonMsg(Json::FastWriter().write(jsonMsg), fd);
}

/**
 * 发送字符串消息
 * @param msg 要发送的字符串消息
 * @param fd 目标套接字文件描述符
 * @return 发送是否成功
 */
bool sendJsonMsg(const std::string& msg, SocketFD fd) {
    if (msg.empty())
        return false;

    int len = msg.length();
    std::string msgSend;
    msgSend.reserve(12 + len);
    msgSend += "length:";
    msgSend += std::to_string(len);
    // 根据消息长度调整格式
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

/**
 * 接收JSON消息
 * @param root 用于存储接收到的JSON消息
 * @param fd 源套接字文件描述符
 * @return 接收状态（-1：失败，0：不完整消息，1：成功）
 */
int recvJsonMsg(Json::Value& root, SocketFD fd) {
    char temp[8] = { 0 };
    char lengthStr[5] = { 0 };
    char msgLengthInfo[13] = { 0 };

    // 接收消息长度信息
    int n = recv(fd, msgLengthInfo, 12, 0);
    if (n <= 0)
        return -1;
    if (n < 12)
        return 0;
    
    // 获取消息长度 （length:）
    memcpy(temp, msgLengthInfo, 7);
    memcpy(lengthStr, msgLengthInfo + 7, 4);
    temp[7] = 0;
    if (strcmp(temp, "length:") != 0)
        return 0;

    int msgLength = 0;
    sscanf(lengthStr, "%d", &msgLength);

    char* jsonMsg = new char[msgLength + 1];
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
        // 解析JSON消息到root
        if (!reader.parse(jsonMsg, root))
            break;

        ret = 1;
    } while (false);

    if (ret == 1) {
        printf("Received message:\n%s\n", jsonMsg);
    }

    delete[] jsonMsg;
    return ret;
}

/**
 * 关闭连接
 * @param fd 套接字文件描述符
 */
void closeSocket(SocketFD fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}