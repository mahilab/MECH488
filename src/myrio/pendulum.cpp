#include <Mahi/Daq.hpp>   // for MyRio
#include <Mahi/Robo.hpp>  // for Butterworth
#include <Mahi/Com.hpp>   // for UdpSocket, TcpSocket, TcpListener
#include <Mahi/Util.hpp>  // for CtrlEvent, Timer, Time
#include "Networking.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

// use namespace so we can say foo() instead of mahi::daq::foo() etc.
using namespace mahi::daq;
using namespace mahi::robo;
using namespace mahi::util;

bool g_stop = false;

class Pendulum  {
public:

    Pendulum() {
        auto ctrl_hand = [](CtrlEvent event) { 
            static int count = 0;
            LOG(Warning) << "Ctrl-C Pressed";
            g_stop = true;
            count++;
            if (count == 2)
                abort();
            return true; 
        };
        register_ctrl_handler(ctrl_hand);
    }

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
         // start the control thread
        m_running = true;
        m_ctrlThread = std::thread(&Pendulum::ctrlThreadFunc, this);
        // listen for TCP messages from GUI
        Packet packet;
        while (m_running) {
            packet;
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
                    std::lock_guard<std::mutex> lock(m_mtx);
                    m_status.enabled = true;
                }
                else if (msg == Message::Disable) {
                    std::lock_guard<std::mutex> lock(m_mtx);
                    m_status.enabled = false;
                }
                else if (msg == Message::ChangeMode) {
                    std::lock_guard<std::mutex> lock(m_mtx);
                    m_status.mode = m_status.mode == (int)Mode::Encoder ? (int)Mode::Midori : (int)Mode::Encoder;
                }
                else if (msg == Message::Shutdown) {
                    m_running = false;
                }
            }
            else {
                LOG(Error) << "Unexpected error.";
                m_running = false;
            }
        }
        m_ctrlThread.join();
    }

    virtual double encoderControl(double t, int counts) {
        return 10*std::sin(2*PI*t);
    }

    virtual double midoriControl(double t, double midori_volts) {
        return 10*std::sin(2*PI*t);
    }

    void ctrlThreadFunc() {

        LOG(Info) << "Starting myRIO control thread.";
        // initialize UDP stream
        UdpSocket udp;
        auto result = udp.bind(SERVER_UDP);
        if (result == Socket::Done)
            LOG(Info) << "UPD socket bound to port " << udp.get_local_port();
        State state;
        Packet packet;
        // initialize myRIO       
        MyRio myrio;
        myrio.mspC.encoder.set_channels({0});
        myrio.mspC.encoder.units[0] = 2 * PI / 500.0;
        myrio.mspC.encoder.zero(0);
        myrio.mspC.DO.set_channels({1});
        myrio.enable();
        // timing
        Timer timer(1000_Hz);
        // start the control loop
        while (m_running) {
            Mode mode;
            char enabled;
            // update status
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_status.running = m_running;
                m_status.misses  = (int)timer.get_misses();
                mode             = (Mode)m_status.mode;
                enabled          = m_status.enabled;
            }
            // read inputs
            myrio.read_all();
            state.tick    = timer.get_elapsed_ticks();
            state.time    = timer.get_elapsed_time_ideal().as_seconds();
            state.sense   = myrio.mspC.AI[0];
            state.midori  = myrio.mspC.AI[1];
            state.encoder = myrio.mspC.encoder[0];
            state.enable  = enabled;
            if (mode == Mode::Encoder)
                state.command = encoderControl(state.time, state.encoder);
            else if (mode == Mode::Midori)
                state.command = midoriControl(state.time, state.midori);
            myrio.mspC.AO[0] = enabled ? state.command : 0;
            myrio.mspC.DO[1] = enabled;
            for (int l = 0; l < 4; ++l)
                myrio.LED[l] = enabled;
            myrio.write_all();
            // stream state
            packet.clear();
            packet << state;
            udp.send(packet, CLIENT_IP, CLIENT_UDP);
            if (g_stop)
                m_running = false;
            timer.wait();
        }   
        myrio.mspC.AO[0] = 0;
        myrio.mspC.DO[1] = TTL_LOW;
        for (int l = 0; l < 4; ++l)
            myrio.LED[l] = TTL_LOW;
        myrio.write_all();
        state.tick = -1;
        packet.clear();
        packet << state;
        udp.send(packet, CLIENT_IP, CLIENT_UDP);
        myrio.disable();
        myrio.close();
        LOG(Info) << "Terminating myRIO control thread.";
    }

private:
    TcpSocket        m_tcp;
    std::thread      m_ctrlThread;
    std::mutex       m_mtx;
    std::atomic_bool m_running;
    Status           m_status;
};

int main(int argc, char const *argv[])
{    
    Pendulum pend;
    pend.run();
    return 0;
}
