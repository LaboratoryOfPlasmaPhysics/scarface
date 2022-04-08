#pragma once
#include <stdio.h>
#include <string>
#include <vector>

template <std::size_t columns>
struct NpyFile
{
    FILE* fp;
    std::size_t lines = 0;

    NpyFile(const std::string& fname) { open(fname.c_str()); }

    NpyFile(const char* fname) { open(fname); }

    inline void open(const std::string& fname)
    {
        open(fname.c_str());
    }
    inline void open(const char* fname)
    {
        fp = fopen(fname, "wb");
        for (int i = 0; i < 0x80 - 1; i++)
            fputc(0x20, fp);
        fputc('\n', fp);
        lines=0;
    }

    inline void append(const std::vector<int32_t>& data)
    {
        append(data.data(), std::size(data) * sizeof(int32_t));
    }

    inline void append(const char* data, std::size_t size)
    {
        if (fp)
        {
            fwrite(data, 1, size, fp);
            lines += size / (columns * sizeof(int32_t));
        }
    }

    inline void write_header()
    {
        if (fp)
        {
            fseek(fp, 0, SEEK_SET);
            fprintf(fp, "\x93NUMPY\x01");
            fputc('\0', fp);
            fputc('v', fp);
            fputc('\0', fp);
            fprintf(fp, "{'descr': '<i4', 'fortran_order': False, 'shape': (%lu, %lu), }", lines,
                columns);
        }
    }

    inline void close()
    {
        if (fp)
        {
            write_header();
            fclose(fp);
            fp = nullptr;
        }
    }

    ~NpyFile() { close(); }
};
