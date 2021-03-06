#include "ps4000aApi.h"
#include <picowrapper.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char* argv[])
{
    PicoWrapper p;
    p.start();
    std::this_thread::sleep_for(std::chrono::seconds{10});
    p.stop();
    p.start();
    std::this_thread::sleep_for(std::chrono::seconds{10});
    p.stop();
}
