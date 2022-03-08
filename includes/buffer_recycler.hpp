#pragma once

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <optional>



template <typename T, typename tag=void>
class buffer_recycler
{
    static std::deque<T> m_queue;
    static std::mutex m_mutex;
public:
    static std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(std::size(m_queue))
        {
            std::optional<T> item = std::move(m_queue.back());
            m_queue.pop_back();
            return item;
        }
        return std::nullopt;
    }

    static void push(T&& buffer)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(buffer));
    }

};

template <typename T, typename tag>
std::deque<T> buffer_recycler<T, tag>::m_queue = {};

template <typename T, typename tag>
std::mutex buffer_recycler<T, tag>::m_mutex = {};
