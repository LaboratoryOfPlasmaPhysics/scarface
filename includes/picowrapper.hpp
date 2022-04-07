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

struct scm_data
{
    int32_t x;
    int32_t y;
    int32_t z;
};

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
        std::memcpy(p->out[i].data() + p->avail_data,
            p->in.data() + startIndex + (i * p->buff_size),
            noOfSamples * sizeof(decltype(p->in.front())));
    }
    p->avail_data += noOfSamples;
}

namespace
{
// constexpr auto all_chan = { PS4000A_CHANNEL_A, PS4000A_CHANNEL_B, PS4000A_CHANNEL_C,
// PS4000A_CHANNEL_D,
//     PS4000A_CHANNEL_E, PS4000A_CHANNEL_F, PS4000A_CHANNEL_G, PS4000A_CHANNEL_H };

constexpr auto all_chan = { PS4000A_CHANNEL_A, PS4000A_CHANNEL_B, PS4000A_CHANNEL_C,
    PS4000A_CHANNEL_D, PS4000A_CHANNEL_E, PS4000A_CHANNEL_F };

template <typename T>
auto set_channel(int16_t handle, T channel)
{
    return ps4000aSetChannel(
        handle, channel, 1, PS4000A_COUPLING::PS4000A_DC, PICO_X1_PROBE_10V, 0.);
}

template <auto channel>
auto set_channels(int16_t handle)
{
    return ps4000aSetChannel(
        handle, channel, 1, PS4000A_COUPLING::PS4000A_DC, PICO_X1_PROBE_10V, 0.);
}

template <auto channel, auto channel_next, auto... channels>
auto set_channels(int16_t handle)
{
    return ps4000aSetChannel(
               handle, channel, 1, PS4000A_COUPLING::PS4000A_DC, PICO_X1_PROBE_10V, 0.)
        | set_channels<channel_next, channels...>(handle);
}

}

template <typename T, std::size_t... I>
auto sum_impl(const T* input, std::index_sequence<I...>)
{
    return (input[I] + ...);
}

template <std::size_t count, typename T>
inline auto sum(const T* input)
{
    return sum_impl(input, std::make_index_sequence<count> {});
}

class PicoWrapper
{
public:
    static constexpr auto fw_donwsampling = 1;
    static constexpr auto donwsampling = 16;
    static constexpr auto channels_count = 6;
    static constexpr auto buffer_size = 256 * 1024;
    using buffer_pool = buffer_recycler<std::vector<scm_data>, struct pico_buff>;

private:
    int16_t handle = -1;
    bool p_ready { false };
    std::vector<int16_t> buffers;
    std::array<std::vector<int16_t>, channels_count> buffer_cpy;
    uint32_t sample_dt = 100;
    std::thread receive_loop;
    std::atomic<bool> running { false };

public:
    std::array<channels::channel<std::vector<scm_data>>, 2> SCM;

    template <int x_idx, int y_idx, int z_idx, std::size_t R, typename T>
    inline void copy_data(T& scm, const std::array<std::vector<int16_t>, channels_count>& buffer,
        std::size_t avail_data)
    {
        std::vector<scm_data> data;
        auto buff = buffer_pool::pop();
        if (buff)
        {
            data = std::move(*buff);
        }
        data.resize(avail_data / R);
        for (auto i = 0UL; i < avail_data / R; i++)
        {
            data[i].x = sum<R>(&(buffer[x_idx][i * R]));
            data[i].y = sum<R>(&(buffer[y_idx][i * R]));
            data[i].z = sum<R>(&(buffer[z_idx][i * R]));
        }
        scm.add(std::move(data));
    }

    PicoWrapper(uint32_t sampling_preiod_ns = 100) : sample_dt { sampling_preiod_ns }
    {
        buffers.resize(buffer_size * channels_count);
        for (auto i = 0UL; i < channels_count; i++)
        {
            buffer_cpy[i].resize(buffer_size);
        }
        auto r = ps4000aOpenUnit(&handle, nullptr);
        if (r == PICO_OK)
        {
            int ch_index = 0;
            for (auto ch : all_chan)
            {
                set_channel(handle, ch);
                r = ps4000aSetDataBuffer(handle, ch, buffers.data() + (ch_index * buffer_size),
                    buffer_size, 0, PS4000A_RATIO_MODE_NONE);
                ch_index += 1;
            }

            if (r == PICO_OK)
            {
                p_ready = true;
            }
            //            ps4000aSetSigGenBuiltIn(handle, 1000, 1000, PS4000A_SQUARE, 10.,
            //            10000., 1., 0.01,
            //                PS4000A_UP, PS4000A_ES_OFF, 0, 0, PS4000A_SIGGEN_RISING,
            //                PS4000A_SIGGEN_NONE, 0);
            // ps4000aSetSigGenBuiltIn(handle, 0, 1000000, PS4000A_SINE, 10000., 10000., 0., 0.,
            //    PS4000A_UP, PS4000A_ES_OFF, 0, 0, PS4000A_SIGGEN_RISING, PS4000A_SIGGEN_NONE, 0);
        }
    }

    ~PicoWrapper()
    {
        if (handle != -1 and p_ready == true)
            ps4000aStop(handle);
    }

    bool ready() const { return this->p_ready; }

    bool start()
    {

        ps4000aRunStreaming(handle, &sample_dt, PS4000A_NS, 0, 0, 0, fw_donwsampling,
            PS4000A_RATIO_MODE_NONE, buffer_size);

        receive_loop = std::thread { [&]()
            {
                running.store(true);
                callback_payload<channels_count> p { buffers, buffer_cpy, 0, buffer_size };
                std::vector<scm_data> scm1_data;
                std::vector<scm_data> scm2_data;
                while (running.load())
                {
                    while (p.avail_data < buffer_size)
                    {
                        ps4000aGetStreamingLatestValues(handle, data_handler<channels_count>, &p);
                    }
                    if (p.avail_data > 0)
                    {
                        copy_data<0, 1, 2, donwsampling>(SCM[0], p.out, p.avail_data);
                        copy_data<3, 4, 5, donwsampling>(SCM[1], p.out, p.avail_data);
                    }
                    p.avail_data = 0;
                }
            } };
        pthread_setname_np(receive_loop.native_handle(), " receive_loop thread");
        return true;
    }

    bool stop()
    {
        running.store(false);
        if (receive_loop.joinable())
            receive_loop.join();
        for (auto& scm : SCM)
        {
            while (scm.size())
            {
                auto buff = scm.take();
                if (buff)
                    buffer_pool::push(std::move(*buff));
            }
        }
        ps4000aStop(handle);
        return true;
    }

    double sampling_frequency() const
    {
        return 1e9 / (this->sample_dt * fw_donwsampling * donwsampling);
    }

    double voltage_resolution() const { return 10. / (donwsampling * 32767.); }

    bool is_running() { return running.load(); }
};
