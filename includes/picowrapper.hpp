#pragma once

#include "buffer_recycler.hpp"
#include "ps4000aApi.h"

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include <channels/channels.hpp>


template <std::size_t ch_count>
struct callback_payload
{
    std::vector<int16_t>& in;
    std::array<std::vector<int16_t>, ch_count>& out;
    std::size_t avail_data;
    std::size_t buff_size;
};

template <std::size_t ch_count>
void data_handler(int16_t handle, int32_t noOfSamples, uint32_t startIndex, int16_t overflow,
    uint32_t triggerAt, int16_t triggered, int16_t autoStop, void* pParameter)
{
    auto p = reinterpret_cast<callback_payload<ch_count>*>(pParameter);
    for (auto i = 0UL; i < ch_count; i++)
    {
        std::memcpy(p->out[i].data(), p->in.data() + startIndex + (i * p->buff_size),
            noOfSamples * sizeof(decltype(p->in.front())));
    }
    p->avail_data = noOfSamples;
}

namespace
{
// constexpr auto all_chan = { PS4000A_CHANNEL_A, PS4000A_CHANNEL_B, PS4000A_CHANNEL_C,
// PS4000A_CHANNEL_D,
//     PS4000A_CHANNEL_E, PS4000A_CHANNEL_F, PS4000A_CHANNEL_G, PS4000A_CHANNEL_H };

constexpr auto all_chan = { PS4000A_CHANNEL_A };

template <auto channel>
auto set_channels(int16_t handle)
{
    return ps4000aSetChannel(
        handle, channel, 1, PS4000A_COUPLING::PS4000A_DC, PICO_X1_PROBE_1V, 0.);
}

template <auto channel, auto channel_next, auto... channels>
auto set_channels(int16_t handle)
{
    return ps4000aSetChannel(handle, channel, 1, PS4000A_COUPLING::PS4000A_DC, PICO_X1_PROBE_1V, 0.)
        | set_channels<channel_next, channels...>(handle);
}

}


template <auto... scope_channels>
class PicoWrapper
{
public:
    static constexpr auto channels_count = sizeof...(scope_channels);
    using buffer_pool = buffer_recycler<std::vector<int16_t>, struct pico_buff>;

private:
    int16_t handle = -1;
    bool ready { false };
    std::vector<int16_t> buffers;
    std::array<std::vector<int16_t>, channels_count> buffer_cpy;
    std::size_t buffer_size;
    std::array<PS4000A_CHANNEL, channels_count> channels { scope_channels... };
    uint32_t sample_dt = 100;
    std::thread receive_loop;
    std::atomic<bool> running { false };

public:
    std::array<channels::channel<std::vector<int16_t>>, channels_count> outputs;
    PicoWrapper(std::size_t buffer_size = 1024 * 1024) : buffer_size { buffer_size }
    {
        buffers.resize(buffer_size * channels_count);
        for (auto i = 0UL; i < channels_count; i++)
        {
            buffer_cpy[i].resize(buffer_size);
        }
        auto r = ps4000aOpenUnit(&handle, nullptr);
        if (r == PICO_OK)
        {


            r = set_channels<scope_channels...>(handle);
            {
                int ch_index = 0;
                for (auto ch : channels)
                {
                    r = ps4000aSetDataBuffer(handle, ch, buffers.data() + (ch_index * buffer_size),
                        buffer_size, 0, PS4000A_RATIO_MODE_AVERAGE);
                    ch_index += 1;
                }
            }
            r = ps4000aRunStreaming(handle, &sample_dt, PS4000A_NS, 0, 0, 0, 4,
                PS4000A_RATIO_MODE_AVERAGE, buffer_size);

            if (r == PICO_OK)
            {
                ready = true;
            }
        }
    }

    ~PicoWrapper()
    {
        if (handle != -1 and ready == true)
            ps4000aStop(handle);
    }

    bool start()
    {

        receive_loop = std::thread { [&]()
            {
                running.store(true);
                callback_payload<channels_count> p { buffers, buffer_cpy, 0, buffer_size };
                while (running.load())
                {
                    ps4000aGetStreamingLatestValues(handle, data_handler<channels_count>, &p);
                    if (p.avail_data)
                    {
                        for (auto j = 0UL; j < channels_count; j++)
                        {
                            std::vector<int16_t> data;
                            auto buff = buffer_pool::pop();
                            if (buff)
                            {
                                data = std::move(*buff);
                            }
                            data.resize(p.avail_data);
                            memcpy(data.data(), p.out[j].data(),
                                p.avail_data * sizeof(decltype(data[0])));
                            outputs[j].add(std::move(data));
                        }
                    }
                }
            } };
        return true;
    }

    bool stop()
    {
        running.store(false);
        for (auto j = 0UL; j < channels_count; j++)
        {
            while (outputs[j].size())
            {
                auto buff = outputs[j].take();
                if (buff)
                    buffer_pool::push(std::move(*buff));
            }
        }
        if (receive_loop.joinable())
            receive_loop.join();
        return true;
    }
};
