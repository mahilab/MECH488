#include <Mahi/Daq.hpp>   // for MyRio
#include <Mahi/Robo.hpp>  // for Butterworth
#include <Mahi/Com.hpp>   // for UdpSocket, TcpSocket, TcpListener
#include <Mahi/Util.hpp>  // for CtrlEvent, Timer, Time
#include "Networking.hpp"
#include <thread>
#include <mutex>
#include <atomic>

// use namespace so we can say foo() instead of mahi::daq::foo() etc.
using namespace mahi::daq;
using namespace mahi::robo;
using namespace mahi::util;

class Pendulum  {
public:
    ~Pendulum() {
        m_tcp.disconnect();
    }

    void run() {
        // wait for TCP connection from GUI client
        LOG(Info) << "Waiting for GUI to connect ...";
        TcpListener listener;
        listener.listen(SERVER_TCP, SERVER_IP);
        if (listener.accept(m_tcp) != Socket::Done) {
            LOG(Error) << "Failed to connect to GUI.";
            return;
        }
        LOG(Info) << "Connected to GUI: " << m_tcp.get_remote_port() << "@" << m_tcp.get_remote_address();
        m_running = true;
        m_ctrlThread = std::thread(&Pendulum::ctrlThreadFunc, this);
        // listen for TCP messages from GUI
        while (m_running) {
            Packet packet;
            auto status = m_tcp.receive(packet);
            if (status == Socket::Disconnected) {
                LOG(Info) << "GUI disconnected.";
                m_running = false;
            }
            else if (status == Socket::Done) {
                int msg;
                packet >> msg;
                if (msg == Message::Ping) {
                    packet.clear();
                    {
                        std::lock_guard<std::mutex> lock(m_mtx);
                        packet << m_status;
                    }
                    m_tcp.send(packet);
                }
                else if (msg == Message::Enable) {
                    LOG(Info) << "Enabling Pendulum";
                }
                else if (msg == Message::Disable) {
                    LOG(Info) << "Disabling Pendulum";
                }
            }
            else {
                LOG(Error) << "Unexpected error.";
                m_running = false;
            }
        }
        m_ctrlThread.join();
    }

    void ctrlThreadFunc() {
        LOG(Info) << "Starting myRIO control thread.";
        UdpSocket udp;
        auto result = udp.bind(SERVER_UDP);
        if (result == Socket::Done)
            LOG(Info) << "UPD socket bound to port " << udp.get_local_port();
        MyRio myrio;
        Timer timer(1000_Hz);
        Time  t;
        State state;
        Packet packet;
        while (m_running) {
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_status.enabled = myrio.is_enabled();
                m_status.running = m_running;
                m_status.misses  = (int)timer.get_misses();
            }
            state.tick = timer.get_elapsed_ticks();
            state.time = t.as_seconds();
            packet.clear();
            packet << state;
            udp.send(packet, CLIENT_IP, CLIENT_UDP);
            myrio.read_all();
            myrio.write_all();
            t = timer.wait();
        }   
        myrio.close();
        LOG(Info) << "Terminating myRIO control thread.";
    }

private:
    TcpSocket        m_tcp;
    std::thread      m_ctrlThread;
    std::mutex       m_mtx;
    std::atomic_bool m_running;

    // mutex protected data
    Status           m_status;
};

int main(int argc, char const *argv[])
{    
    Pendulum pend;
    pend.run();
    return 0;
}
