#include "SnifferPannel.hpp"
#include <FFT/FFT.hpp>
#include <QVector>
constexpr auto sp_size = 4096;
using fft_t = FFT::FFT<sp_size * 2, double>;

SnifferPannel::SnifferPannel(QWidget* parent)
        : QWidget(parent), wf_plot { new Plot }, fft_plot { new Plot }, spectro_plot { new Plot }
{
    this->setLayout(new QVBoxLayout);
    this->layout()->addWidget(wf_plot);
    this->layout()->addWidget(fft_plot);
    this->layout()->addWidget(spectro_plot);

    this->update_thread = std::thread(
        [this]()
        {
            std::vector<double> mod_data;
            double avg=0.;
            while (!this->input.closed())
            {
                auto maybe_data = this->input.take();
                if (maybe_data)
                {
                    auto data = std::move(*maybe_data);
                    QVector<QCPGraphData> qcp_wf_data(std::size(data));

                    std::for_each(std::cbegin(data), std::cend(data),
                        [i = 0,&avg , &qcp_wf_data, &mod_data,&fft_plot=this->fft_plot](const measurement& m) mutable
                        {
                            int32_t x = m.x, y = m.y, z = m.z;
                            float mod = sqrtf(x * x + y * y + z * z);
                            qcp_wf_data[i] = QCPGraphData(i, mod);
                            mod_data.push_back(mod);
                            avg+=mod;
                            if(std::size(mod_data)==fft_t::fft_size)
                            {
                                avg/=fft_t::fft_size;
                                std::transform(std::cbegin(mod_data),std::cend(mod_data),std::begin(mod_data),[avg](const double v){return v-avg;});
                                auto spect=fft_t::mod(fft_t::fft(mod_data),true);
                                mod_data.resize(0);
                                QVector<QCPGraphData> qcp_spec_data(std::size(spect));
                                std::for_each(std::cbegin(spect), std::cend(spect),
                                    [i = 0, &qcp_spec_data](double m) mutable
                                    {
                                        qcp_spec_data[i] = QCPGraphData(i, m);
                                        i+=1;
                                    });
                                QMetaObject::invokeMethod(
                                    fft_plot, [data=std::move(qcp_spec_data),fft_plot = fft_plot]() { fft_plot->set_data(data); },
                                    Qt::QueuedConnection);
                            }

                            i += 1;
                        });
                    buffer_pool::push(std::move(data));
                    QMetaObject::invokeMethod(
                        wf_plot, [data=std::move(qcp_wf_data),wf_plot = this->wf_plot]() { wf_plot->set_data(data); },
                        Qt::QueuedConnection);
                }
            }
        });
}

Plot::Plot() : QCustomPlot()
{
    this->setPlottingHint(QCP::phFastPolylines, true);
    this->addGraph();
    this->setInteractions(QCP::Interaction::iRangeDrag|QCP::Interaction::iRangeZoom|QCP::Interaction::iSelectAxes);
    SciQLopPlots::removeAllMargins(this);
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
            }
                event->accept();
                break;
        }
    }
}



void Plot::set_data(const QVector<QCPGraphData>& data)
{
    this->graph()->data()->set(data, true);
    this->replot(QCustomPlot::rpQueuedReplot);
}

