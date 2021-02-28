#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>
#include "Networking.hpp"
#include <thread>
#include <mutex>
#include <atomic>

using namespace mahi::gui;
using namespace mahi::util;
using namespace mahi::com;

template <typename T> 
struct DataBuffer {
    DataBuffer(int max_size = 20000) : max_size(max_size) {
        offset = 0;
        data.reserve(max_size);
    }
    void push_back(T v) {
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
    int          offset;
    ImVector<T>  data;
private:
    const int    max_size;
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
    void show_logs();
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
    SPSCQueue<State>      m_states;
    int                   m_msgSent   = 0;
    int                   m_packsRecv = 0;
    int                   m_packsLost = 0;
private:
    DataBuffer<double> m_timeData;
    DataBuffer<double> m_senseData;
    DataBuffer<double> m_commandData;
    DataBuffer<double> m_midoriData;
    DataBuffer<double> m_encoderData;
    DataBuffer<double> m_enableData;
};
