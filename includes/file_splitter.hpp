#pragma once
#include "npy.hpp"
#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <ctime>
#include <iomanip>

std::string now()
{
    using namespace std::literals;
    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream strm;
    strm << std::put_time(std::localtime(&t_c), "%FT%T");
    return strm.str();
}

template <std::size_t columns, std::size_t max_lines>
struct FileSplitter
{

    std::unique_ptr<NpyFile<columns>> _numpy_file;
    const std::string basename;
    std::size_t lines = 0;

    FileSplitter(std::string basename) : basename { basename } { }

    FileSplitter(const char* basename) : basename { basename } { }

    inline void open()
    {
        const auto fname = basename + now() + std::string { ".npy" };
        if (_numpy_file)
        {
            _numpy_file->close();
            _numpy_file->open(fname);
        }
        else
            _numpy_file = std::make_unique<NpyFile<columns>>(fname);
        lines = 0;
    }


    inline void append(const std::vector<int32_t>& data)
    {
        _numpy_file->append(data);
        lines += (std::size(data) / columns);
        if (lines >= max_lines) [[unlikely]]
            open();
    }

    inline void append(const char* data, std::size_t size)
    {
        _numpy_file->append(data, size);
        lines += (size / (columns * sizeof(int32_t)));
        if (lines >= max_lines) [[unlikely]]
            open();
    }

    inline void close()
    {
        if (_numpy_file)
            _numpy_file->close();
        lines = 0;
    }

    ~FileSplitter()
    {
    }
};
