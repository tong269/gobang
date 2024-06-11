{
    depfiles_gcc = "json_writer.o: src/jsoncpp/json_writer.cpp src/jsoncpp/json_tool.h  src/jsoncpp/json/config.h src/jsoncpp/json/allocator.h  src/jsoncpp/json/version.h src/jsoncpp/json/writer.h  src/jsoncpp/json/value.h src/jsoncpp/json/forwards.h  src/jsoncpp/json/config.h\
",
    files = {
        "src/jsoncpp/json_writer.cpp"
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