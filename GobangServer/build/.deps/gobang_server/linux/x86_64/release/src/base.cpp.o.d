{
    depfiles_gcc = "base.o: src/base.cpp src/base.h\
",
    files = {
        "src/base.cpp"
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