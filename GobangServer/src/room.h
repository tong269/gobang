#pragma once

#include "base.h"
#include "player.h"
#include "socket_func.h"
#include "thread_pool.h"
#include "jsoncpp/json/json.h"

#include <string>
#include <vector>

#define MAX_NUM_WATCHERS 20


class Room {
public:
    Room();
    ~Room() {}

public:
    void initChessBoard();                                                  //初始化棋盘

    int getId() const { return id; }                                        //获取房间ID
    void setId(int idIn) { id = idIn; }                                     //设置房间ID
    const std::string& getName() const { return name; }                     //获取房间名字
    void setName(const std::string& nameIn) { name = nameIn; }              //设置房间名字 

    int getNumPlayers() const { return numPlayers; }                        //获取房间玩家数量
    int getNumWatchers() const { return watchers.size(); }                  //获取观战人数
    bool isFull() const { return watchers.size() >= MAX_NUM_WATCHERS; }     //房间是否满了

    void addPlayer(const std::string& name, SocketFD fd);                   //添加一名玩家
    void addWatcher(const std::string& name, SocketFD fd);                  //添加一名观众
    void quitPlayer(SocketFD fd);                                           //踢出一名玩家
    void quitWatcher(SocketFD fd);                                          //踢出一名观众

    Player& getPlayer1() { return player1; }                                //获取玩家1
    Player& getPlayer2() { return player2; }                                //获取玩家2
    Player* getPlayer(SocketFD fd);                                         //通过玩家的socket找到玩家
    Player* getRival(int my_fd);                                            //获取对手

    bool shouldDelete() const { return flagShouldDelete; }                  //是否应该删除房间

private:
    void setPiece(int row, int col, ChessType type);                        //放置棋子

    bool parseJsonMsg(const Json::Value& root, SocketFD fd);                //解析json消息

    bool processMsgTypeCmd(const Json::Value& root, SocketFD fd);           //处理控制命令
    bool processMsgTypeResponse(const Json::Value& root, SocketFD fd);      //处理响应
    bool processMsgTypeChat(const Json::Value& root, SocketFD fd);          //处理聊天
    bool processMsgTypeNotify(const Json::Value& root, SocketFD fd);        //处理通知命令

    bool processNotifyRivalInfo(const Json::Value& root, SocketFD fd);      //向玩家发送他的对手的信息
    bool processPrepareGame(const Json::Value& root, SocketFD fd);          //处理准备游戏
    bool processCancelPrepareGame(const Json::Value& root, SocketFD fd);    //处理开始游戏
    bool processStartGame(const Json::Value& root, SocketFD fd);            //
    bool processNewPiece(const Json::Value& root, SocketFD fd);             //新的对局
    bool processGameOver(const Json::Value& root, SocketFD fd);             //游戏结束
    bool processExchangeChessType(const Json::Value& root, SocketFD fd);    //交换黑白

    enum GameStatus {
        GAME_RUNNING = 1,
        GAME_PREPARE = 2,
        GAME_END = 3
    };

private:
    ThreadPool pool;                    //线程池，一个房间一个线程池
    bool flagShouldDelete = false;      //退出标识

    int chessPieces[15][15];            //棋盘
    GameStatus gameStatus = GAME_END;   //当前游戏状态
    ChessPieceInfo lastChess;           //上次落子

    int id = -1;                        //房间号
    std::string name;                   //房间名字
    int numPlayers = 0;                 //玩家数量

    Player player1;                     //玩家1
    Player player2;                     //玩家2

    std::vector<Watcher> watchers;      //观众
};

