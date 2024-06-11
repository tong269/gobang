#include "room.h"

#include "api.h"
#include "jsoncpp/json/json.h"

#include <algorithm>
#include <iostream>
#include <string>

#define Log(x) std::cout << (x) << std::endl


Room::Room() :
    pool(MAX_NUM_WATCHERS + 2)
{
    initChessBoard();
    lastChess = { 0, 0, CHESS_NULL };
}

void Room::initChessBoard() {
    for (int row = 0; row < 15; ++row) {
        for (int col = 0; col < 15; ++col) {
            chessPieces[row][col] = 0;
        }
    }
}

//放置棋子
void Room::setPiece(int row, int col, ChessType type) {
    if (row < 0 || row > 14 || col < 0 || col > 14)
        return;
    chessPieces[row][col] = type;
}

//将玩家加入到房间
void Room::addPlayer(const std::string& name, SocketFD fd) {
    //添加第一个玩家
    if (numPlayers == 0) {
        player1 = Player(name, fd, CHESS_BLACK);
        numPlayers++;
    }
    else {//第二个玩家
        player2 = Player(name, fd, reverse(player1.type));
        numPlayers++;
    }
    //通知所有观战的玩家，有新玩家加入
    for (auto& watcher : watchers) {
        API::notifyPlayerInfo(watcher.socketfd, player1.name, player1.type,
                player2.name, player2.type);
    }
    //一个玩家加入到房间后，就会有一个线程为其一直阻塞，处理该玩家发送的消息
    pool.enqueue([this, fd](){
        while (1) {
            Json::Value root;
            int ret = recvJsonMsg(root, fd);
            if (ret == -1) {
                Log("Quit");
                quitPlayer(fd);
                return;
            }
            else if (ret > 0){
                parseJsonMsg(root, fd);
            }
        }
    });
}
//添加观众
void Room::addWatcher(const std::string& name, SocketFD fd) {
    watchers.emplace_back(name, fd);
    //向该观众发送对局双方信息
    API::notifyPlayerInfo(fd, player1.name, player1.type, player2.name, player2.type);
    //发送棋盘信息
    API::sendChessBoard(fd, this->chessPieces, lastChess);
    //为该观众分配一个线程
    pool.enqueue([this, fd](){
        std::cout << "add watcher" << std::endl;
        while (1) {
            Json::Value root;
            //接收该观众发来的消息
            int ret = recvJsonMsg(root, fd);
            std::cout << "recv watcher msg" << std::endl;
            if (ret == -1) {
                //接收消息失败 踢出该观众
                Log("A watcher quit");
                quitWatcher(fd);
                return;
            }
            else if (ret > 0){
                std::cout << "recv watcher msg" << std::endl;
                //类型检测 观众只能发送chat类型的消息
                if (root["type"].isNull() || root["type"].asString() != "chat" ||
                        root["message"].isNull() || root["sender"].isNull()) {
                    return; 
                }
                //向其他的观众发送该消息
                for (auto& watcher : watchers) {
                    if (watcher.socketfd != fd) {
                        API::forward(watcher.socketfd, root);
                    }
                }
                //向棋手发送
                if (numPlayers >= 1)
                    API::forward(player1.socketfd, root);
                if (numPlayers == 2)
                    API::forward(player2.socketfd, root);
            }
        }
    });
}
//踢出玩家
void Room::quitPlayer(SocketFD fd) {
    std::cout << "quit player" << std::endl;
    std::cout << numPlayers << std::endl;
    // won't happen
    if (numPlayers <= 0)
        return;

    std::string quitPlayerName = getPlayer(fd)->name;

    //如果踢出的是房主，则另一位玩家应该接管
    if (fd == player1.socketfd) {
        if (numPlayers == 2)
            player1 = player2; 
    }

    numPlayers--;
    //没有玩家，房间应该删除
    if (numPlayers == 0) {
        flagShouldDelete = true;
    }
    else {
        //告知房间中的其他人
        API::notifyDisconnect(player1.socketfd, quitPlayerName);
        for (auto& watcher : watchers)
            API::notifyDisconnect(watcher.socketfd, quitPlayerName);
        gameStatus = GAME_END;
        lastChess = { 0, 0, CHESS_NULL };
    }

    std::cout << numPlayers << std::endl;
    closeSocket(fd);
}
//踢出观众
void Room::quitWatcher(SocketFD fd) {
    std::cout << "quit watcher" << std::endl;
    std::cout << watchers.size() << std::endl;

    //从观众列表中删除
    for (auto it = watchers.begin(); it != watchers.end(); ++it) {
        if (it->socketfd == fd) {
            watchers.erase(it);
            return;
        }
    }

    closeSocket(fd);
    std::cout << watchers.size() << std::endl;
}
//解析json消息，执行对应函数
bool Room::parseJsonMsg(const Json::Value& root, SocketFD fd) {
    if (root["type"].isNull())
        return false;
    std::string msgType = root["type"].asString();//获取消息类型 "command" "response" "chat" "notify"
    
    if (msgType == "command") { //"command" 有三种类型，准备，取消准备，交换黑白                     
        if (root["cmd"].isNull())
            return false;
        else
            return processMsgTypeCmd(root, fd);
    }
    if (msgType == "response") { //响应信息，响应是否同意交换黑白
        if (root["res_cmd"].isNull())
            return false;
        else
            return processMsgTypeResponse(root, fd);
    }
    if (msgType == "chat") //聊天类型
        return processMsgTypeChat(root, fd);
    if (msgType == "notify") { // 通知类型
        if (root["sub_type"].isNull())
            return false;
        else
            return processMsgTypeNotify(root, fd);
    }

    return false;
}
//玩家加入后游戏开始前，对应三种操作  准备 | 取消准备| 交换
bool Room::processMsgTypeCmd(const Json::Value& root, SocketFD fd) {
    std::string cmd = root["cmd"].asString();
    if (cmd == "prepare")
        return processPrepareGame(root, fd);
    if (cmd == "cancel_prepare")
        return processCancelPrepareGame(root, fd);
    if (cmd == "exchange")
        return processExchangeChessType(root, fd);
    return true;
}

bool Room::processMsgTypeResponse(const Json::Value& root, SocketFD fd) {
    std::string res_cmd = root["res_cmd"].asString();
    if (res_cmd == "prepare") {
        if (root["accept"].isNull())
            return false;

        //同一交换
        if (root["accept"].asBool()) {
            ChessType tmp = player1.type;
            player1.type = player2.type;
            player2.type = tmp;
        }
    }
    return API::forward(getRival(fd)->socketfd, root);
}

bool Room::processMsgTypeChat(const Json::Value& root, SocketFD fd) {

    //发送消息给房间内其他人
    for (auto& watcher : watchers) {
        if (watcher.socketfd != fd) {
            API::forward(watcher.socketfd, root);
        }
    }
    return API::forward(getRival(fd)->socketfd, root); 
}

/*
    Accept one connection.
    Recieved message:
    {"cmd":"create_room","player_name":"xfd","room_name":"test","type":"command"}

    Recv Ret: 1
    Sending message
    length:82  
    {"desc":"OK","res_cmd":"create_room","room_id":5455,"status":0,"type":"response"}


    Accept one connection.
    Recieved message:
    {"cmd":"join_room","player_name":"xfd2","room_id":5455,"type":"command"}

    Recv Ret: 1
    Sending message
    length:101 
    {"desc":"","res_cmd":"join_room","rival_name":"xfd","room_name":"test","status":0,"type":"response"}

    Sending message
    length:63  
    {"player_name":"xfd2","sub_type":"rival_info","type":"notify"}

*/

bool Room::processMsgTypeNotify(const Json::Value& root, SocketFD fd) {
    std::string subType = root["sub_type"].asString();
    if (subType == "new_piece")             //落子
        return processNewPiece(root, fd);
    if (subType == "game_over")             //游戏结束
        return processGameOver(root, fd);
    if (subType == "rival_info")            //对手信息
        return processNotifyRivalInfo(root, fd);
    return false;
}

bool Room::processNotifyRivalInfo(const Json::Value& root, SocketFD fd) {
    Player* player = getPlayer(fd);
    if (!root["player_name"].isNull())
        player->name = root["player_name"].asString();

    return API::forward(getRival(fd)->socketfd, root);
}
//处理准备事件
//如果房间只有一名玩家，则告知该玩家需等待对手加入
//如果房间有两名玩家，两名玩家都准备好时，修改room的状态为 game_running , 如果另外一名玩家还没准备好， game_prepare
bool Room::processPrepareGame(const Json::Value& root, SocketFD fd) {
    Player* player = getPlayer(fd);

    if (numPlayers == 2) {
        player->prepare = true;
        //都准备好了
        if (player1.prepare && player2.prepare) {
            gameStatus = GAME_RUNNING;
            initChessBoard();
            lastChess = { 0, 0, CHESS_NULL };
            player1.prepare = false;
            player2.prepare = false;

            //通知房间内所有人游戏开始了
            API::notifyGameStart(fd);
            for (auto& watcher : watchers)
                API::notifyGameStart(watcher.socketfd);
            return API::notifyGameStart(getRival(fd)->socketfd);
        }
        //对手还没准备
        else {
            gameStatus = GAME_PREPARE;
            API::responsePrepare(fd, STATUS_OK, "OK");
            for (auto& watcher : watchers)
                API::forward(watcher.socketfd, root);
            return API::forward(getRival(fd)->socketfd, root);
        }
    }
    //人数不够
    else {
        player->prepare = false;
        std::string desc = "Please wait for the other player to join";
        //通知该玩家，等待加入
        return API::responsePrepare(fd, STATUS_ERROR, desc);
    }
}

bool Room::processCancelPrepareGame(const Json::Value& root, SocketFD fd) {
    getPlayer(fd)->prepare = false;

    //通知所有人
    for (auto& watcher : watchers)
        API::notifyGameCancelPrepare(watcher.socketfd);
    return  API::notifyGameCancelPrepare(getRival(fd)->socketfd);
}


bool Room::processNewPiece(const Json::Value& root, SocketFD fd) {
    if (root["row"].isNull() || root["col"].isNull() || root["chess_type"].isNull())
        return false;
    int row = root["row"].asInt();
    int col = root["col"].asInt();
    int chessType = root["chess_type"].asInt();
    setPiece(row, col, ChessType(chessType));   //落子
    lastChess = { row, col, chessType };
    //向房间内观众发送落子信息
    for (auto& watcher : watchers) {
        API::notifyNewPiece(watcher.socketfd, row, col, chessType);
    }
    //向对手发送
    return API::notifyNewPiece(getRival(fd)->socketfd, row, col, chessType);
}

bool Room::processGameOver(const Json::Value& root, SocketFD fd) {
    if (root["game_result"].isNull())
        return false;

    if (root["game_result"].asString() == "win") {
        if (root["chess_type"].isNull())
            return false;
        //黑棋赢了还是白棋
        ChessType winChessType = ChessType(root["chess_type"].asInt());
    }
    else {  //Draw

    }

    gameStatus = GAME_END;
    std::cout << "game over" << std::endl;
    return true;
}

bool Room::processExchangeChessType(const Json::Value& root, SocketFD fd) {
    if (numPlayers != 2)
        return false;
    //通知对手
    return API::forward(getRival(fd)->socketfd, root);
}

Player* Room::getPlayer(SocketFD fd) {
    if (player1.socketfd == fd)
        return &player1;
    else if (player2.socketfd == fd)
        return &player2;
    else
        return nullptr;
}

Player* Room::getRival(int my_fd) {
    if (player1.socketfd == my_fd)
        return &player2;
    else if (player2.socketfd == my_fd)
        return &player1;
    return nullptr;
}


