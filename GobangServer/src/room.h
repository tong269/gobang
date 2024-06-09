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
    void initChessBoard();

    int getId() const { return id; }
    void setId(int idIn) { id = idIn; }
    const std::string& getName() const { return name; }
    void setName(const std::string& nameIn) { name = nameIn; }

    int getNumPlayers() const { return numPlayers; }
    int getNumWatchers() const { return watchers.size(); }
    bool isFull() const { return watchers.size() >= MAX_NUM_WATCHERS; }

    void addPlayer(const std::string& name, SocketFD fd);
    void addWatcher(const std::string& name, SocketFD fd);
    void quitPlayer(SocketFD fd);
    void quitWatcher(SocketFD fd);

    Player& getPlayer1() { return player1; }
    Player& getPlayer2() { return player2; }
    Player* getPlayer(SocketFD fd);
    Player* getRival(int my_fd);

    bool shouldDelete() const { return flagShouldDelete; }

private:
    void setPiece(int row, int col, ChessType type);

    bool parseJsonMsg(const Json::Value& root, SocketFD fd);

    bool processMsgTypeCmd(const Json::Value& root, SocketFD fd);
    bool processMsgTypeResponse(const Json::Value& root, SocketFD fd);
    bool processMsgTypeChat(const Json::Value& root, SocketFD fd);
    bool processMsgTypeNotify(const Json::Value& root, SocketFD fd);

    bool processNotifyRivalInfo(const Json::Value& root, SocketFD fd);
    bool processPrepareGame(const Json::Value& root, SocketFD fd);
    bool processCancelPrepareGame(const Json::Value& root, SocketFD fd);
    bool processStartGame(const Json::Value& root, SocketFD fd);
    bool processNewPiece(const Json::Value& root, SocketFD fd);
    bool processGameOver(const Json::Value& root, SocketFD fd);
    bool processExchangeChessType(const Json::Value& root, SocketFD fd);

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

