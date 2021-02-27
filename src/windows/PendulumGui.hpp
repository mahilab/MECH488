#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>
#include "Networking.hpp"
#include <thread>
#include <mutex>
#include <atomic>

using namespace mahi::gui;
using namespace mahi::util;
using namespace mahi::com;

class PendulumGui : public Application {
public:
    PendulumGui();
    ~PendulumGui();

private:
    void update() override;

    bool connect();
    bool ping();
    bool send_message(Message msg);

    void dataThreadFunc();

    void show_network();
    void show_logs();
    void show_cmds();
    void show_status();

    void style_gui();

private:
    TcpSocket         m_tcp;
    std::atomic_bool  m_connected;
    Status            m_status;
    std::thread       m_dataThread;
    int               m_msgSent   = 0;
    int               m_packsRecv = 0;
    int               m_packsLost = 0;
};
