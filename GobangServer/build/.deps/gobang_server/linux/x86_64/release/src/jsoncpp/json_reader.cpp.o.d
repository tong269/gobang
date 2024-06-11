{
    depfiles_gcc = "json_reader.o: src/jsoncpp/json_reader.cpp src/jsoncpp/json_tool.h  src/jsoncpp/json/config.h src/jsoncpp/json/allocator.h  src/jsoncpp/json/version.h src/jsoncpp/json/assertions.h  src/jsoncpp/json/config.h src/jsoncpp/json/reader.h  src/jsoncpp/json/json_features.h src/jsoncpp/json/forwards.h  src/jsoncpp/json/value.h src/jsoncpp/json/value.h\
",
    files = {
        "src/jsoncpp/json_reader.cpp"
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