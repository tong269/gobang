#include "api.h"
#include "socket_func.h"

#include "jsoncpp/json/writer.h"
#include <iostream>

namespace API {


/***********************
 * 本地函数
***********************/ 
// 发送简单命令的函数
static bool sendSimpleCmd(SocketFD fd, const std::string& cmd) {
    Json::Value root;
    root["type"] = "command"; // 设置消息类型为命令
    root["cmd"] = cmd; // 设置命令内容
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送简单响应的函数
static bool sendSimpleRes(SocketFD fd, const std::string& res_cmd,
        int status_code, const std::string desc) {
    Json::Value root;
    root["type"] = "response"; // 设置消息类型为响应
    root["res_cmd"] = res_cmd; // 设置响应命令
    root["status"] = status_code; // 设置状态码
    root["desc"] = desc; // 设置描述信息
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送简单通知的函数
static bool sendSimpleNotify(SocketFD fd, const std::string& sub_type) {
    Json::Value root;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = sub_type; // 设置通知子类型
    return sendJsonMsg(root, fd); // 发送JSON消息
}


/******************************************
 * 检查接收到的消息类型
******************************************/
// 检查消息是否为指定命令类型
bool isTypeCommand(const Json::Value& root, const std::string& cmd) {
    return !root["type"].isNull() && root["type"].asString() == "command" // 检查消息类型是否为命令
        && !root["cmd"].isNull() && root["cmd"].asString() == cmd; // 检查命令内容是否匹配
}

// 检查消息是否为指定通知类型
bool isTypeNotify(const Json::Value& root, const std::string& sub_type) {
    return !root["type"].isNull() && root["type"].asString() == "notify" // 检查消息类型是否为通知
        && !root["sub_type"].isNull() && root["sub_type"] == sub_type; // 检查通知子类型是否匹配
}


/**************************************
 * 转发给其他玩家
**************************************/
// 将消息转发给其他玩家
bool forward(SocketFD fd, const Json::Value& root) {
    return sendJsonMsg(root, fd); // 直接转发JSON消息
}


/***********************
 * 类型: 响应
***********************/
// 创建房间的响应函数
bool responseCreateRoom(SocketFD fd, int status_code, const std::string& desc, int room_id) {
    Json::Value root;
    root["type"] =  "response"; // 设置消息类型为响应
    root["res_cmd"] = "create_room"; // 设置响应命令为创建房间
    root["status"] = status_code; // 设置状态码
    root["desc"] = desc; // 设置描述信息
    root["room_id"] = room_id; // 设置房间ID
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 加入房间的响应函数
bool responseJoinRoom(SocketFD fd, int status_code, const std::string &desc,
        const std::string room_name, const std::string rival_name) {
    Json::Value root;
    root["type"] = "response"; // 设置消息类型为响应
    root["res_cmd"] = "join_room"; // 设置响应命令为加入房间
    root["status"] = status_code; // 设置状态码
    root["desc"] = desc; // 设置描述信息
    root["room_name"] = room_name; // 设置房间名称
    root["rival_name"] = rival_name; // 设置对手名称
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 观看房间的响应函数
bool responseWatchRoom(SocketFD fd, int status_code, const std::string& desc,
        const std::string room_name) {
    Json::Value root;
    root["type"] = "response"; // 设置消息类型为响应
    root["res_cmd"] = "watch_room"; // 设置响应命令为观看房间
    root["status"] = status_code; // 设置状态码
    root["desc"] = desc; // 设置描述信息
    root["room_name"] = room_name; // 设置房间名称
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 准备的响应函数
bool responsePrepare(SocketFD fd, int status_code, const std::string& desc) {
    return sendSimpleRes(fd, "prepare", status_code, desc); // 使用sendSimpleRes发送准备响应
}


/*************************
 * 类型: 通知
*************************/
// 发送棋盘布局和最后一步棋的通知函数
bool sendChessBoard(SocketFD fd, int chessPieces[][15], ChessPieceInfo last_piece) {
    Json::Value root;
    Json::Value chessboard;
    Json::Value lastChess;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = "chessboard"; // 设置通知子类型为棋盘

    for (int row = 0; row < 15; ++row) {
        for (int col = 0; col < 15; ++col) {
            chessboard.append(chessPieces[row][col]); // 添加棋盘布局
        }
    }
    root["layout"] = chessboard; // 设置棋盘布局

    lastChess["row"] = last_piece.row; // 设置最后一个棋子的行
    lastChess["col"] = last_piece.col; // 设置最后一个棋子的列
    lastChess["type"] = last_piece.type; // 设置最后一个棋子的类型
    root["last_piece"] = lastChess; // 设置最后一个棋子的信息

    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送对手信息的通知函数
bool notifyRivalInfo(SocketFD fd, const std::string& player_name) {
    Json::Value root;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = "rival_info"; // 设置通知子类型为对手信息
    root["player_name"] = player_name; // 设置对手名称
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送新棋子信息的通知函数
bool notifyNewPiece(SocketFD fd, int row, int col, int chess_type) {
    Json::Value root;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = "new_piece"; // 设置通知子类型为新棋子
    root["row"] = row; // 设置新棋子的行
    root["col"] = col; // 设置新棋子的列
    root["chess_type"] = chess_type; // 设置新棋子的类型
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送游戏开始的通知函数
bool notifyGameStart(SocketFD fd) {
    return sendSimpleNotify(fd, "game_start"); // 发送游戏开始通知
}

// 发送取消准备的通知函数
bool notifyGameCancelPrepare(SocketFD fd) {
    return sendSimpleNotify(fd, "cancel_prepare"); // 发送取消准备通知
}

// 发送断开连接的通知函数
bool notifyDisconnect(SocketFD fd, const std::string& player_name) {
    Json::Value root;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = "disconnect"; // 设置通知子类型为断开连接
    root["player_name"] = player_name; // 设置玩家名称
    return sendJsonMsg(root, fd); // 发送JSON消息
}

// 发送玩家信息的通知函数
bool notifyPlayerInfo(SocketFD fd, const std::string& player1_name, int player1_chess_type,
        const std::string& player2_name, int player2_chess_type) {
    Json::Value root;
    root["type"] = "notify"; // 设置消息类型为通知
    root["sub_type"] = "player_info"; // 设置通知子类型为玩家信息
    root["player1_name"] = player1_name; // 设置玩家1名称
    root["player1_chess_type"] = player1_chess_type; // 设置玩家1棋子类型
    root["player2_name"] = player2_name; // 设置玩家2名称
    root["player2_chess_type"] = player2_chess_type; // 设置玩家2棋子类型
    return sendJsonMsg(root, fd); // 发送JSON消息
}



}; // namespace API

