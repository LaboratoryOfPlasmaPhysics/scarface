#include <Graph.hpp>
#include <QApplication>
#include <QCustomPlotWrapper.hpp>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <SciQLopPlot.hpp>
#include <channels/pipelines.hpp>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <qcp.h>
#include <buffer_recycler.hpp>

using data_t = std::pair<std::vector<double>, std::vector<double>>;
using data2d_t = std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;
struct measurement
{
    int16_t x;
    int16_t y;
    int16_t z;
};

class Plot:public QCustomPlot
{
    std::mutex m_mutex;
public:
    Plot();
    void keyPressEvent(QKeyEvent* event) override;
    void set_data(const QVector<QCPGraphData>& data);
};

class SnifferPannel: public QWidget
{
    Plot* wf_plot;
    Plot* fft_plot;
    Plot* spectro_plot;
    std::thread update_thread;
public:
    using buffer_pool = buffer_recycler<std::vector<measurement>>;
    channels::channel<std::vector<measurement>, 1, channels::full_policy::overwrite_last> input;
    SnifferPannel(QWidget*parent=nullptr);

};
