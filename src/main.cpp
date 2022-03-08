#include "SnifferPannel.hpp"
#include <FFT/FFT.hpp>
#include <Graph.hpp>
#include <QApplication>
#include <QCustomPlotWrapper.hpp>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QRandomGenerator>
#include <SciQLopPlot.hpp>
#include <channels/pipelines.hpp>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <picowrapper.hpp>
#include <thread>

using namespace channels::operators;
using namespace channels;
using namespace std::placeholders;

std::array colors_list = { Qt::blue, Qt::red, Qt::green, Qt::lightGray, Qt::darkBlue };

using PicoWrapper_t = PicoWrapper<PS4000A_CHANNEL_A, PS4000A_CHANNEL_B, PS4000A_CHANNEL_C>;

int main(int argc, char* argv[])
{

    QApplication app { argc, argv };
    QMainWindow mainw;
    mainw.setCentralWidget(new QWidget);
    auto wdgt = mainw.centralWidget();
    auto pps_label = new QLabel();
    SnifferPannel* pan = new SnifferPannel;
    wdgt->setLayout(new QVBoxLayout);
    wdgt->layout()->addWidget(pan);

    PicoWrapper_t pico_scope { 128 * 1024 * 1024 / 4 };


    pico_scope.start();
    constexpr std::size_t channels_count = decltype(pico_scope)::channels_count;
    auto th = std::thread(
        [&pico_scope, pan]()
        {
            while (!pico_scope.outputs[0].closed() && !pico_scope.outputs[1].closed()
                && !pico_scope.outputs[2].closed())
            {
                std::array<std::vector<int16_t>, 3> buffers;
                for (auto i = 0UL; i < channels_count; i++)
                {
                    auto v = pico_scope.outputs[i].take();
                    if (v)
                    {
                        buffers[i] = std::move(*v);
                    }
                    else
                    {
                        return;
                    }
                }
                std::vector<measurement> plot_buffer;
                if (auto b = SnifferPannel::buffer_pool::pop(); b)
                    plot_buffer = std::move(*b);
                plot_buffer.resize(std::size(buffers[0]));
                for (auto i = 0UL; i < std::size(buffers[0]); i++)
                {
                    plot_buffer[i] = { buffers[0][i], buffers[1][i], buffers[2][i] };
                }
                pan->input.add(std::move(plot_buffer));
                for (auto i = 0UL; i < channels_count; i++)
                {
                    PicoWrapper_t::buffer_pool::push(std::move(buffers[i]));
                }
            }
        });


    mainw.setMinimumSize(1024, 768);
    mainw.show();
    return app.exec();
}
