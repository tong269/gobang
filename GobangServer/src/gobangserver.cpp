//服务器实现
#include "gobangserver.h"

#include "api.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>


// GobangServer构造函数，初始化线程池并启动管理房间的线程
GobangServer::GobangServer() : pool(10) {
#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(sockVersion, &wsaData); // Windows平台初始化套接字库
#endif

    // 启动一个线程来管理房间
    pool.enqueue([this](){
        while (isRunning) {
            std::cout << "Rooms in use: " << rooms.size() << std::endl;
            for (auto it = rooms.begin(); it != rooms.end(); ) {
                if ((*it)->shouldDelete()) {
                    std::cout << "Deleting room: " << (*it)->getId() << std::endl;                
                    delete (*it); // 删除需要删除的房间
                    it = rooms.erase(it); // 从列表中移除房间
                }
                else {
                    ++it;
                }
            }

            // 如果没有房间，休眠5秒；否则，休眠1秒
            if (rooms.size() == 0)
                std::this_thread::sleep_for(std::chrono::seconds(5));
            else
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}


// 停止服务器，关闭套接字并停止运行标志
void GobangServer::stop() {
    std::cout << "\nStopping server" << std::endl;
    shutdown(socketfd, 2); // 关闭套接字的读写
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Closing socket" << std::endl;
    closeSocket(socketfd); // 关闭套接字
    isRunning = false; // 设置运行标志为false
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Quit successfully" << std::endl;
}

// 启动服务器，绑定端口并监听连接
// port 端口号
bool GobangServer::start(int port) {
    socketfd = socket(AF_INET, SOCK_STREAM, 0); //创建套接字，用于监听
    if (socketfd <= 0) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return false;
    }

    
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    //绑定网卡
    if (bind(socketfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        closeSocket(socketfd);
        return false;
    }

    //监听端口
    if (listen(socketfd, 10) == -1) {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        closeSocket(socketfd);
        return false;
    }

    printf("Running...\n");
    
    while (isRunning) {
        SocketFD connectfd = 0;
        //接收连接
        if ((connectfd = accept(socketfd, (struct sockaddr*)NULL, NULL)) <= 0) {
            printf("accept socket error %s(errno: %d)\n", strerror(errno), errno);
            continue;
        }

        printf("Accept one connection.\n");

        //封装一下，放进线程池，通知一个线程来执行匿名函数，解析请求
        pool.enqueue([this, connectfd](){
            Json::Value root;
            int ret = recvJsonMsg(root, connectfd);//接收消息
            std::cout << "Recv Ret: " << ret << std::endl;;

            if (ret == -1) {
                closeSocket(connectfd);//接收失败就关闭客户端
                return;
            }
            else if (ret > 0){
                parseJsonMsg(root, connectfd);//解析消息
            }
        });
    }

    return true;
}

// 解析JSON消息
bool GobangServer::parseJsonMsg(const Json::Value& root, SocketFD fd) {
    if (root["type"].isNull()) {
        return false;
    }

    std::string type = root["type"].asString();

    //创建房间、加入房间、观战
    if (type == "command") {
        return processMsgTypeCmd(root, fd);
    }

    return true;
}

bool GobangServer::processMsgTypeCmd(const Json::Value& root, SocketFD fd) {
    if (root["cmd"].isNull())
        return false;

    std::string cmd = root["cmd"].asString();

    if (cmd == "create_room")//创建房间
        return processCreateRoom(root, fd);
    if (cmd == "join_room")//加入房间
        return processJoinRoom(root, fd);
    if (cmd == "watch_room")//观战
        return processWatchRoom(root, fd);

    return false;
}


//创建一个房间，然后把创建房间的用户加入房间
bool GobangServer::processCreateRoom(const Json::Value& root, SocketFD fd) {
    //检查是否填写了房间名和玩家名，没有则直接返回
    if (root["room_name"].isNull() || root["player_name"].isNull())
        return false;
    //创建房间，并初始化线程池
    Room* room = createRoom();
    //添加到房间数组里面
    rooms.push_back(room);
    room->setName(root["room_name"].asString());
    //添加玩家
    room->addPlayer(root["player_name"].asString(), fd);

    return API::responseCreateRoom(fd, 0, "OK", room->getId());
}

//处理玩家加入房间的请求
bool GobangServer::processJoinRoom(const Json::Value& root, SocketFD fd) {
    if (root["room_id"].isNull() || root["player_name"].isNull())
        return false;

    int roomId = root["room_id"].asInt();
    std::string playerName = root["player_name"].asString();
    Room* room = getRoomById(roomId);

    int statusCode = STATUS_ERROR;
    std::string desc, roomName, rivalname;

    do {
        // 房间不存在
        if (!room) {
            desc = "The room is not exist";
            break;
        }

        //获取对战玩家的人数，看看是否可以加入
        int numPlayers = room->getNumPlayers();

        //人数够了
        if (numPlayers == 2) {
            desc = "The room is full";
            break;
        }
        else if (numPlayers == 1) {
            Player& player1 = room->getPlayer1();
            //检查一下名字是否相同
            if (player1.name == playerName) {
                desc = "The name of two players cant't be same";
                break;
            }
            //不同，记录新加入玩家的名字
            roomName = room->getName();
            rivalname = player1.name;
        }
        else {
            desc = "Server Internal Error. Please try to join another room";
            break;
        }
        //加入房间
        room->addPlayer(playerName, fd);
        statusCode = STATUS_OK;
    } while (false);

    // response new player
    API::responseJoinRoom(fd, statusCode, desc, roomName, rivalname);

    if (room && statusCode == STATUS_OK)
        // notify old player
        return API::notifyRivalInfo(room->getPlayer1().socketfd, playerName);
    else
        return false;
}

// 处理玩家观战的请求
bool GobangServer::processWatchRoom(const Json::Value& root, SocketFD fd) {
    if (root["room_id"].isNull() || root["player_name"].isNull())
        return false;

    int roomId = root["room_id"].asInt();
    std::string playerName = root["player_name"].asString();
    Room* room = getRoomById(roomId);
    if (room) {
        API::responseWatchRoom(fd, STATUS_OK, "", room->getName());
        room->addWatcher(playerName, fd); 
    }
    else{
        API::responseWatchRoom(fd, STATUS_ERROR, "The room is not exist", "");
    }

    return true;
}

// 实现删除房间的逻辑
bool GobangServer::processDeleteRoom(const Json::Value& root, SocketFD fd) {

    return true;
}


/*****************************************************************************/
/*****************************************************************************/

// 创建一个新房间
Room* GobangServer::createRoom() {
    if (rooms.size() > 100)
        return nullptr;

    Room* room = new Room();

    while (true) {
        //获取一个随机数作为房间id
        int randNum = rand() % 9000 + 1000;
        auto iter = std::find(roomsId.begin(), roomsId.end(), randNum);
        if (iter == roomsId.end()) {
            roomsId.push_back(randNum);
            room->setId(randNum);
            break;
        }
    }

    return room;
}

// 根据房间ID获取房间对象
Room* GobangServer::getRoomById(int id) {
    for (int i = 0, size = rooms.size(); i < size; ++i) {
        if (rooms[i]->getId() == id)
            return rooms[i];
    }
    return nullptr;
}


