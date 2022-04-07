#include "SnifferPannel.hpp"
#include <FFT/FFT.hpp>
#include <QVector>
constexpr auto sp_size = 4096;
using fft_t = FFT::FFT<sp_size * 2, double>;

inline void removeAllMargins(QWidget* widget)
{
    widget->setContentsMargins(0, 0, 0, 0);
    auto layout = widget->layout();
    if (layout)
    {
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->setContentsMargins(0, 0, 0, 0);
    }
}


SnifferPannel::SnifferPannel(double voltage_resolution, double sampling_frequency, QWidget* parent)
        : QWidget(parent)
        , wf_plot { new Plot(voltage_resolution, sampling_frequency, 3) }
        , fft_plot { new Plot(voltage_resolution, sampling_frequency, 3) }
        , spectro_plot { new ColorMapPlot(voltage_resolution, sampling_frequency) }
        , sampling_frequency { sampling_frequency }
{
    this->setLayout(new QVBoxLayout);
    this->layout()->addWidget(wf_plot);
    this->layout()->addWidget(fft_plot);
    this->layout()->addWidget(spectro_plot);
    fft_plot->set_log_y(true);
    fft_plot->set_log_x(true);

    this->update_thread = std::thread(
        [this, voltage_resolution]()
        {
            std::vector<double> x_fft_in;
            std::vector<double> y_fft_in;
            std::vector<double> z_fft_in;
            double x_avg = 0., y_avg = 0., z_avg = 0;
            while (!this->input.closed())
            {
                auto maybe_data = this->input.take();
                if (maybe_data)
                {
                    auto data = std::move(*maybe_data);
                    QVector<QCPGraphData> qcp_wf_data_x(std::size(data));
                    QVector<QCPGraphData> qcp_wf_data_y(std::size(data));
                    QVector<QCPGraphData> qcp_wf_data_z(std::size(data));

                    std::for_each(std::cbegin(data), std::cend(data),
                        [i = 0, &x_avg, &y_avg, &z_avg, &qcp_wf_data_x, &qcp_wf_data_y,
                            &qcp_wf_data_z, &x_fft_in, &y_fft_in, &z_fft_in,
                            &fft_plot = this->fft_plot, &ffts = this->ffts,
                            &sampling_frequency = this->sampling_frequency,
                            voltage_resolution](const scm_data& m) mutable
                        {
                            const float x = voltage_resolution * m.x, y = voltage_resolution * m.y,
                                        z = voltage_resolution * m.z;
                            qcp_wf_data_x[i] = QCPGraphData(i, x);
                            qcp_wf_data_y[i] = QCPGraphData(i, y);
                            qcp_wf_data_z[i] = QCPGraphData(i, z);
                            x_fft_in.push_back(x);
                            y_fft_in.push_back(y);
                            z_fft_in.push_back(z);
                            x_avg += x;
                            y_avg += y;
                            z_avg += z;
                            if (std::size(x_fft_in) == fft_t::fft_size)
                            {
                                x_avg /= fft_t::fft_size;
                                y_avg /= fft_t::fft_size;
                                z_avg /= fft_t::fft_size;
                                for (int i = 0; i < fft_t::fft_size; i++)
                                {
                                    x_fft_in[i] -= x_avg;
                                    y_fft_in[i] -= y_avg;
                                    z_fft_in[i] -= z_avg;
                                }
                                auto x_spect = fft_t::fft(x_fft_in);
                                auto y_spect = fft_t::fft(y_fft_in);
                                auto z_spect = fft_t::fft(z_fft_in);
                                x_fft_in.resize(0);
                                y_fft_in.resize(0);
                                z_fft_in.resize(0);
                                x_avg = 0;
                                y_avg = 0;
                                z_avg = 0;
                                QVector<QCPGraphData> qcp_x_spec_data(fft_t::fft_size / 2 + 1);
                                QVector<QCPGraphData> qcp_y_spec_data(fft_t::fft_size / 2 + 1);
                                QVector<QCPGraphData> qcp_z_spec_data(fft_t::fft_size / 2 + 1);
                                std::vector<double> sum_spec(fft_t::fft_size / 2 + 1);
                                for (int i = 0; i < fft_t::fft_size / 2 + 1; i++)
                                {
                                    const double x = x_spect[i].real() * x_spect[i].real()
                                        + x_spect[i].imag() * x_spect[i].imag();
                                    const double y = y_spect[i].real() * y_spect[i].real()
                                        + y_spect[i].imag() * y_spect[i].imag();
                                    const double z = z_spect[i].real() * z_spect[i].real()
                                        + z_spect[i].imag() * z_spect[i].imag();
                                    qcp_x_spec_data[i] = QCPGraphData(
                                        i * (sampling_frequency / fft_t::fft_size), x);
                                    qcp_y_spec_data[i] = QCPGraphData(
                                        i * (sampling_frequency / fft_t::fft_size), y);
                                    qcp_z_spec_data[i] = QCPGraphData(
                                        i * (sampling_frequency / fft_t::fft_size), z);
                                    sum_spec[i] = x + y + z;
                                }
                                QMetaObject::invokeMethod(
                                    fft_plot,
                                    [data = std::move(qcp_x_spec_data), fft_plot = fft_plot]()
                                    { fft_plot->set_data(data, 0); },
                                    Qt::QueuedConnection);
                                QMetaObject::invokeMethod(
                                    fft_plot,
                                    [data = std::move(qcp_y_spec_data), fft_plot = fft_plot]()
                                    { fft_plot->set_data(data, 1); },
                                    Qt::QueuedConnection);
                                QMetaObject::invokeMethod(
                                    fft_plot,
                                    [data = std::move(qcp_z_spec_data), fft_plot = fft_plot]()
                                    { fft_plot->set_data(data, 2); },
                                    Qt::QueuedConnection);


                                ffts.add(std::move(sum_spec));
                            }

                            i += 1;
                        });
                    buffer_pool::push(std::move(data));
                    QMetaObject::invokeMethod(
                        wf_plot,
                        [data = std::move(qcp_wf_data_x), wf_plot = this->wf_plot]()
                        { wf_plot->set_data(data, 0); },
                        Qt::QueuedConnection);
                    QMetaObject::invokeMethod(
                        wf_plot,
                        [data = std::move(qcp_wf_data_y), wf_plot = this->wf_plot]()
                        { wf_plot->set_data(data, 1); },
                        Qt::QueuedConnection);
                    QMetaObject::invokeMethod(
                        wf_plot,
                        [data = std::move(qcp_wf_data_z), wf_plot = this->wf_plot]()
                        { wf_plot->set_data(data, 2); },
                        Qt::QueuedConnection);
                }
            }
        });

    this->spectrogram_thread = std::thread(
        [this]()
        {
            int avg = 0;
            std::vector<double> avg_spec(fft_t::fft_size / 2 + 1);
            while (!this->ffts.closed())
            {
                auto maybe_ffts = this->ffts.take();
                constexpr auto avg_cnt = 16;
                if (maybe_ffts)
                {
                    auto ffts = std::move(*maybe_ffts);
                    std::transform(std::cbegin(ffts), std::cend(ffts), std::cbegin(avg_spec),
                        std::begin(avg_spec), std::plus<double> {});
                    avg += 1;
                    if (avg >= avg_cnt)
                    {
                        std::transform(std::cbegin(avg_spec), std::cend(avg_spec),
                            std::begin(avg_spec),
                            [](double v) { return 10 * log10((v + 1e-4) / avg_cnt); });
                        avg = 0;
                        QMetaObject::invokeMethod(
                            spectro_plot,
                            [avg_spec = std::move(avg_spec), spectro_plot = this->spectro_plot]()
                            { spectro_plot->add_data(avg_spec); },
                            Qt::QueuedConnection);
                        avg_spec = std::vector<double>(fft_t::fft_size / 2 + 1);
                    }
                }
            }
        });
    pthread_setname_np(this->update_thread.native_handle(), "SnifferPannel thread");
}

SnifferPannel::~SnifferPannel()
{
    this->input.close();
    if (this->update_thread.joinable())
        this->update_thread.join();
    ffts.close();
    if (this->spectrogram_thread.joinable())
        this->spectrogram_thread.join();
}

void SnifferPannel::update_sampling_frequency(double sampling_frequency)
{
    this->sampling_frequency = sampling_frequency;
    this->spectro_plot->update_sampling_frequency(sampling_frequency);
}

std::array colors_list = { Qt::blue, Qt::red, Qt::green, Qt::lightGray, Qt::darkBlue };


Plot::Plot(double voltage_resolution, double sampling_frequency, int graph_count) : QCustomPlot()
{
    this->setPlottingHint(QCP::phFastPolylines, true);
    while (graph_count--)
    {
        auto graph = this->addGraph();
        graph->setAdaptiveSampling(true);
        auto pen = graph->pen();
        pen.setColor(colors_list[graph_count]);
        graph->setPen(pen);
    }
    this->setInteractions(QCP::Interaction::iRangeDrag | QCP::Interaction::iRangeZoom
        | QCP::Interaction::iSelectAxes);
    removeAllMargins(this);
    this->plotLayout()->setMargins(QMargins { 0, 0, 0, 0 });
    this->plotLayout()->setRowSpacing(0);
    for (auto rect : this->axisRects())
    {
        rect->setMargins(QMargins { 0, 0, 0, 0 });
    }
}


void Plot::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::NoModifier)
    {
        switch (event->key())
        {
            case Qt::Key_M:
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                this->xAxis->rescale(true);
                this->yAxis->rescale(true);
                this->replot(QCustomPlot::rpQueuedReplot);
            }
                event->accept();
                break;
        }
    }
}


void Plot::set_data(const QVector<QCPGraphData>& data, int graph_index)
{
    this->graph(graph_index)->data()->set(data, true);
    this->replot(QCustomPlot::rpQueuedReplot);
}

void Plot::set_log_y(bool log)
{
    if (log)
    {
        this->yAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        this->yAxis->setTicker(logTicker);
    }
    else
    {
        this->yAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        this->yAxis->setTicker(linTicker);
    }
}

void Plot::set_log_x(bool log)
{
    if (log)
    {
        this->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        this->xAxis->setTicker(logTicker);
    }
    else
    {
        this->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        this->xAxis->setTicker(linTicker);
    }
}

ColorMapPlot::ColorMapPlot(double voltage_resolution, double sampling_frequency)
        : Plot(voltage_resolution, sampling_frequency, 0), sampling_frequency { sampling_frequency }
{
    colorMap = new QCPColorMap(this->xAxis, this->yAxis);
    colorMap->data()->setSize(fft_t::fft_size / 32, fft_t::fft_size / 2);
    colorMap->data()->setRange(
        QCPRange(0, colorMap->data()->keySize()), QCPRange(0, sampling_frequency / 2));
    colorMap->setGradient(QCPColorGradient::gpJet);
    colorMap->rescaleDataRange(true);
    colorMap->setInterpolate(false);
    this->rescaleAxes();
}

void ColorMapPlot::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::NoModifier)
    {
        switch (event->key())
        {
            case Qt::Key_M:
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                colorMap->rescaleDataRange(true);
                this->rescaleAxes();
                this->replot(QCustomPlot::rpQueuedReplot);
            }
                event->accept();
                break;
        }
    }
}

void ColorMapPlot::add_data(const std::vector<double>& data)
{
    const auto sz = std::min(std::size(data), fft_t::fft_size / 2 + 1);
    const auto df = sampling_frequency / (2 * sz);
    for (auto i = 0UL; i < sz; i++)
    {
        colorMap->data()->setData(index, i * df, data[i]);
    }
    index = (index + 1) % colorMap->data()->keySize();
    for (auto i = 0UL; i < std::min(std::size(data), fft_t::fft_size / 2 + 1); i++)
    {
        colorMap->data()->setData(index, i * df, 0);
    }
    this->replot(QCustomPlot::rpQueuedReplot);
}

void ColorMapPlot::update_sampling_frequency(double sampling_frequency)
{
    this->sampling_frequency = sampling_frequency;
    this->colorMap->data()->setRange(
        QCPRange(0, colorMap->data()->keySize()), QCPRange(0, sampling_frequency / 2));
}
