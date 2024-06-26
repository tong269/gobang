#include "gobangserver.h"

#include "api.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>


GobangServer::GobangServer() :
    pool(10)
{
#ifdef WIN32
        WORD sockVersion = MAKEWORD(2, 2);
        WSADATA wsaData;
        WSAStartup(sockVersion, &wsaData);
#endif

    pool.enqueue([this](){
        while (isRunning) {
            std::cout << "Rooms in use: " << rooms.size() << std::endl;
            for (auto it = rooms.begin(); it != rooms.end(); ) {
                if ((*it)->shouldDelete()) {
                    std::cout << "Deleting room: " << (*it)->getId() << std::endl;                
                    delete (*it);
                    it = rooms.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (rooms.size() == 0)
                std::this_thread::sleep_for(std::chrono::seconds(5));
            else
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}


void GobangServer::stop() {
    std::cout << "\nStopping server" << std::endl;
    shutdown(socketfd, 2);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Closing socket" << std::endl;
    closeSocket(socketfd);
    isRunning = false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Quit successfully" << std::endl;
}


bool GobangServer::start(int port) {
    socketfd = socket(AF_INET, SOCK_STREAM, 0); //创建套接字，用于监听
    if (socketfd <= 0) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return false;
    }

    //IP和端口
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
    //发送响应，创建房间成功
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

    // 成功加入房间后，向发起加入房间请求的玩家通知他加入成功了
    API::responseJoinRoom(fd, statusCode, desc, roomName, rivalname);

    if (room && statusCode == STATUS_OK)
        //通知对手
        return API::notifyRivalInfo(room->getPlayer1().socketfd, playerName);
    else
        return false;
}
//处理观战
bool GobangServer::processWatchRoom(const Json::Value& root, SocketFD fd) {
    if (root["room_id"].isNull() || root["player_name"].isNull())
        return false;

    int roomId = root["room_id"].asInt();
    std::string playerName = root["player_name"].asString();
    Room* room = getRoomById(roomId);
    if (room) {
        //通知发起加入请求的玩家，他加入成功了
        API::responseWatchRoom(fd, STATUS_OK, "", room->getName());
        room->addWatcher(playerName, fd); //向该房间添加观众
    }
    else{
        //房间不存在，通知一下
        API::responseWatchRoom(fd, STATUS_ERROR, "The room is not exist", "");
    }

    return true;
}

bool GobangServer::processDeleteRoom(const Json::Value& root, SocketFD fd) {

    return true;
}


/*****************************************************************************/
/*****************************************************************************/

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
//通过房间ID拿到房间
Room* GobangServer::getRoomById(int id) {
    for (int i = 0, size = rooms.size(); i < size; ++i) {
        if (rooms[i]->getId() == id)
            return rooms[i];
    }
    return nullptr;
}


