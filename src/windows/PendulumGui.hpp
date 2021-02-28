#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>
#include "Common.hpp"
#include <thread>
#include <mutex>
#include <atomic>

using namespace mahi::gui;

struct DataBuffer1D {
    DataBuffer1D(int max_size = 20000) : max_size(max_size) {
        offset = 0;
        data.reserve(max_size);
    }
    void push_back(double v) {
        if (data.size() < max_size)
            data.push_back(v);
        else {
            data[offset] = v;
            offset       = (offset + 1) % max_size;
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
private:
    const int         max_size;
};

struct DataBuffer2D {
    DataBuffer2D(int max_size = 20000) : max_size(max_size) {
        offset = 0;
        data.reserve(max_size);
    }
    void push_back(double x, double y) {
        if (data.size() < max_size)
            data.push_back(ImPlotPoint(x, y));
        else {
            data[offset] = ImPlotPoint(x, y);
            offset       = (offset + 1) % max_size;
        }
    }
    void clear() {
        if (data.size() > 0) {
            data.shrink(0);
            offset = 0;
        }
    }
public:
    int                   offset;
    ImVector<ImPlotPoint> data;
private:
    const int             max_size;
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
    Status                m_status;
    std::thread           m_data_thread;
    int                   m_msgSent   = 0;
    int                   m_packsRecv = 0;
    int                   m_packsLost = 0;
private:
    SPSCQueue<Data> m_queue;
    DataBuffer1D      m_timeData;
    DataBuffer1D      m_senseData;
    DataBuffer1D      m_commandData;
    DataBuffer1D      m_midoriData;
    DataBuffer1D      m_encoderData;
    DataBuffer1D      m_enableData;
    std::map<std::string,std::pair<bool,DataBuffer2D>> m_plots;
};
