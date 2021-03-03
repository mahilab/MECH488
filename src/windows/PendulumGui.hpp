#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>
#include "Common.hpp"
#include <thread>
#include <mutex>
#include <atomic>

#define MAX_SAMPLES 20000

using namespace mahi::gui;

struct DataBuffer {    
    void push_back(double v) {
        if (size < MAX_SAMPLES) {
            data[size++] = v;       
        } 
        else {
            data[offset] = v;
            offset       = (offset + 1) % MAX_SAMPLES;
        }
    }
    void clear() { size = offset = 0; }
public:
    int    size              = 0;
    int    offset            = 0;
    double data[MAX_SAMPLES] = {0};
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
    std::map<std::string,DataBuffer> m_plots;
};
