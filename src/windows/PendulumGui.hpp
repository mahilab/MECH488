#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>
#include "Common.hpp"
#include <thread>
#include <mutex>
#include <atomic>

#define MAX_SAMPLES 20000

using namespace mahi::gui;

struct DataBuffer {
    DataBuffer() {
        offset = 0;
        data.reserve(MAX_SAMPLES);
    }
    void push_back(double v) {
        if (data.size() < MAX_SAMPLES)
            data.push_back(v);
        else {
            data[offset] = v;
            offset       = (offset + 1) % MAX_SAMPLES;
        }
    }
    void clear() {
        if (data.size() > 0) {
            data.shrink(0);
            offset = 0;
        }
    }
public:
    int               offset;
    ImVector<double>  data;
};

class PendulumGui : public Application {
public:
    PendulumGui();
    ~PendulumGui();
private:
    void update() override;
    bool connect();
    bool ping();
    bool send_message(Message msg);
    void data_thread_func();
    void clear_data();
    void export_data(const std::string& filepath);
    void show_network();
    void show_logs(LogBuffer& logs, ImGuiTextFilter& filter, bool& verb);
    void show_cmds();
    void show_status();
    void show_plot();
    void style_gui();
private:
    TcpSocket             m_tcp;
    UdpSocket             m_udp;
    std::atomic_bool      m_connected;
    std::mutex            m_data_mtx;
    Status                m_status;
    std::thread           m_data_thread;
    int                   m_msgSent   = 0;
    int                   m_packsRecv = 0;
    int                   m_packsLost = 0;
private:
    SPSCQueue<Data> m_queue;
    DataBuffer      m_timeData;
    DataBuffer      m_senseData;
    DataBuffer      m_commandData;
    DataBuffer      m_midoriData;
    DataBuffer      m_encoderData;
    DataBuffer      m_enableData;
    std::map<std::string,std::pair<bool,DataBuffer>> m_plots;
};
