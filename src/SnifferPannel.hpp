//#include <Graph.hpp>
#include <QApplication>
//#include <QCustomPlotWrapper.hpp>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
//#include <SciQLopPlot.hpp>
#include <channels/pipelines.hpp>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <qcp.h>
#include <buffer_recycler.hpp>
#include <picowrapper.hpp>

class Plot:public QCustomPlot
{
protected:
    std::mutex m_mutex;
public:
    Plot(double voltage_resolution, double sampling_frequency,int graph_count,int color_offset=0);
    void keyPressEvent(QKeyEvent* event) override;
    void set_data(const QVector<QCPGraphData>& data, int graph_index=0);
    void set_log_y(bool log);
    void set_log_x(bool log);
};

class ColorMapPlot:public Plot
{
    QCPColorMap *colorMap;
    int index=0;
    double sampling_frequency;
public:
    ColorMapPlot(double voltage_resolution, double sampling_frequency);
    void keyPressEvent(QKeyEvent* event) override;
    void add_data(const std::vector<double>& data);
    void update_sampling_frequency(double sampling_frequency);
};

class SnifferPannel: public QWidget
{
    Plot* wf_plot;
    Plot* fft_plot;
    ColorMapPlot* spectro_plot;
    std::thread update_thread;
    std::thread spectrogram_thread;
    double sampling_frequency;
public:
    using buffer_pool = buffer_recycler<std::vector<scm_data>>;
    channels::channel<std::vector<scm_data>, 2, channels::full_policy::overwrite_last> input;
    channels::channel<std::vector<double>, 2, channels::full_policy::overwrite_last> ffts;
    SnifferPannel(double voltage_resolution, double sampling_frequency,int color_offset=0, QWidget*parent=nullptr);
    ~SnifferPannel();
    void update_sampling_frequency(double sampling_frequency);
};
