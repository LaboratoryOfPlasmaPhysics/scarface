#include "SnifferPannel.hpp"
#include "file_splitter.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <channels/pipelines.hpp>
#include <cmath>
#include <cppconfig/cppconfig.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <picowrapper.hpp>
#include <thread>

using namespace channels::operators;
using namespace channels;
using namespace std::placeholders;
using FSpliter = FileSplitter<3, 1024 * 1024 * 32>;

struct TestEnv
{
    QString description;
    QString output_folder;
    double sampling_frequency;
    double quantum;
};


template <typename T, typename U>
inline void scm_handler(T& SCM, U* scm_pan, FSpliter& f)
{
    std::vector<scm_data> buffer;
    if (auto v = SCM.take(); v)
        buffer = std::move(*v);
    else
        return;
    f.append((char*)buffer.data(), std::size(buffer) * sizeof(scm_data));
    std::vector<scm_data> plot_buffer;
    if (auto b = SnifferPannel::buffer_pool::pop(); b)
        plot_buffer = std::move(*b);
    plot_buffer.resize(std::size(buffer));
    std::memcpy(plot_buffer.data(), buffer.data(), std::size(buffer) * sizeof(scm_data));
    scm_pan->input.add(std::move(plot_buffer));
    PicoWrapper::buffer_pool::push(std::move(buffer));
}

void save_context(const QString& path, const TestEnv& env)
{
    cppconfig::Config c;
    c["sampling_frequency"] = env.sampling_frequency;
    c["quantum"] = env.quantum;
    c["description"] = env.description.toStdString();
    c["start_time"] = QDateTime::currentDateTime().toString().toStdString();
    cppconfig::to_json(std::filesystem::path(path.toStdString())/"context.json",c);
}

struct Connector
{
    SnifferPannel *scm1_pan, *scm2_pan;
    PicoWrapper& pico_scope;
    std::thread thread;
    std::atomic<bool> running { false };
    Connector(SnifferPannel* scm1_pan, SnifferPannel* scm2_pan, PicoWrapper& pico_scope)
            : scm1_pan { scm1_pan }, scm2_pan { scm2_pan }, pico_scope { pico_scope }
    {
    }

    void start(const TestEnv env)
    {
        pico_scope.start();
        QString output_folder(
            env.output_folder + '/' + QDateTime::currentDateTime().toString("yyyy-MM-ddThh.mm.ss"));
        if (!QDir(output_folder).exists())
        {
            QDir(output_folder).mkpath(output_folder);
        }

        save_context(output_folder,env);

        this->running.store(true);
        thread = std::thread(
            [this, output_folder]()
            {
                auto f1 = FSpliter((output_folder + "/scm1-").toStdString());
                auto f2 = FSpliter((output_folder + "/scm2-").toStdString());
                f1.open();
                f2.open();

                while (this->running.load() && !pico_scope.SCM[0].closed()
                    && !pico_scope.SCM[1].closed())
                {
                    scm_handler(pico_scope.SCM[0], scm1_pan, f1);
                    scm_handler(pico_scope.SCM[1], scm2_pan, f2);
                }
                pico_scope.stop();
            });
        pthread_setname_np(thread.native_handle(), "Connector_thread");
    }
    void stop()
    {
        this->running.store(false);
        if (thread.joinable())
            thread.join();
    }

    ~Connector() { stop(); }
};

std::string home_folder()
{
    auto home = getenv("HOME");
    if (home)
    {
        return std::string(home);
    }
    return {};
}

cppconfig::Config load_config()
{
    auto home = home_folder();
    if (!home.empty())
    {
        auto cfg = home + "/.config/scarface/config.yaml";
        return cppconfig::from_yaml(std::filesystem::path(cfg));
    }
    return {};
}

int main(int argc, char* argv[])
{

    QApplication app { argc, argv };
    auto config = load_config();
    auto output_folder = config["output_folder"].to<std::string>(home_folder() + "/Juice_data");
    PicoWrapper pico_scope(config["picoscope"]["sampling_period_ns"].to<int>(50));
    QMainWindow mainw;
    if (!pico_scope.ready())
    {
        QMessageBox::critical(
            &mainw, "Scarface Error", "Can't open picoscope check USB and try again");
        return -1;
    }
    mainw.setCentralWidget(new QWidget);
    auto wdgt = mainw.centralWidget();
    auto sampling_freq_label
        = new QLabel(QString::number(pico_scope.sampling_frequency() / 1000.) + "kHz");
    auto start_stop = new QAction("Sart");
    mainw.menuBar()->addAction(start_stop);
    mainw.statusBar()->addWidget(sampling_freq_label);
    SnifferPannel* scm1_pan
        = new SnifferPannel { pico_scope.voltage_resolution(), pico_scope.sampling_frequency() };
    SnifferPannel* scm2_pan
        = new SnifferPannel { pico_scope.voltage_resolution(), pico_scope.sampling_frequency(), 3 };
    Connector connector(scm1_pan, scm2_pan, pico_scope);
    QObject::connect(start_stop, &QAction::triggered,
        [start_stop, &connector, &pico_scope, sampling_freq_label, scm1_pan, scm2_pan,
            mainw = &mainw, &output_folder]()
        {
            if (!pico_scope.is_running())
            {
                auto desc
                    = QInputDialog::getMultiLineText(mainw, "Test description", "description");
                start_stop->setText("Stop");
                connector.start(TestEnv { desc, QString::fromStdString(output_folder),
                    pico_scope.sampling_frequency(), pico_scope.voltage_resolution() });
                const auto fs = pico_scope.sampling_frequency();
                scm1_pan->update_sampling_frequency(fs);
                scm2_pan->update_sampling_frequency(fs);
                sampling_freq_label->setText(QString::number(fs / 1000.) + "kHz");
                while (!pico_scope.is_running())
                    ;
            }
            else
            {
                start_stop->setText("Start");
                connector.stop();
                while (pico_scope.is_running())
                    ;
            }
        });


    wdgt->setLayout(new QHBoxLayout);
    wdgt->layout()->addWidget(scm1_pan);
    wdgt->layout()->addWidget(scm2_pan);

    mainw.setMinimumSize(1024, 768);
    mainw.show();
    return app.exec();
}
