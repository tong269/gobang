{
    depfiles_gcc = "room.o: src/room.cpp src/room.h src/base.h src/player.h src/socket_func.h  src/jsoncpp/json/json.h src/jsoncpp/json/autolink.h  src/jsoncpp/json/config.h src/jsoncpp/json/allocator.h  src/jsoncpp/json/version.h src/jsoncpp/json/json_features.h  src/jsoncpp/json/forwards.h src/jsoncpp/json/reader.h  src/jsoncpp/json/value.h src/jsoncpp/json/writer.h src/thread_pool.h  src/api.h\
",
    files = {
        "src/room.cpp"
    },
    values = {
        "/usr/bin/gcc",
        {
            "-m64",
            "-std=c++14",
            "-Isrc/jsoncpp"
        }
    }
}