#include "IPendulum.hpp"
#include <Mahi/Daq.hpp>   // for MyRio

using namespace mahi::daq;

static bool g_stop = false;
static bool g_zero = false;

template <class Formatter>
class MyRioLogWritter : public Writer {
public:
    MyRioLogWritter(Severity max_severity = Debug) : Writer(max_severity), logs(100) {}

    virtual void write(const LogRecord& record) override {
        auto log = std::pair<Severity, std::string>(record.get_severity(), Formatter::format(record));
        logs.push_back(log);
    }
    LogBuffer logs;
};

static MyRioLogWritter<TxtFormatter> remote_writer;

class RateMonitor {
public:
    RateMonitor(mahi::util::Time updateInterval = mahi::util::seconds(1)) : 
        m_updateInterval(updateInterval),
        m_nextUpdateTime(m_updateInterval),
        m_rate(0), m_ticks(0)
    { }
    void tick() {
        m_ticks++;
    }
    void update(const mahi::util::Time& t) {
        if (t > m_nextUpdateTime) {
            m_rate = m_ticks / m_updateInterval.as_seconds();
            m_nextUpdateTime += m_updateInterval;
            m_ticks = 0;
        }
    }
    double rate() const {
        return m_rate;
    }
private:
    Time m_updateInterval;
    Time m_nextUpdateTime;
    double m_rate;
    double m_ticks;
};


IPendulum::IPendulum() : 
    m_running(false)
{
    if (MahiLogger) {
        MahiLogger->add_writer(&remote_writer);
        MahiLogger->set_max_severity(Debug);
    }
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

IPendulum::~IPendulum() {
    // does nothing
}

void IPendulum::run(Frequency loop_rate) {
    if (m_running)
    {
        LOG(Warning) << "The pendulum controller is already running!";
        return;
    }
    // wait for TCP connection from GUI client
    LOG(Info) << "Waiting for GUI to connect ...";
    TcpSocket   tcp;
    TcpListener listener;
    listener.listen(SERVER_TCP, SERVER_IP);
    if (listener.accept(tcp) != Socket::Done) {
        LOG(Error) << "Failed to connect to GUI.";
        return;
    }
    LOG(Info) << "Connected to GUI: " << tcp.get_remote_port() << "@" << tcp.get_remote_address();
        // start the control thread
    m_running = true;
    m_ctrl_thread = std::thread(&IPendulum::ctrl_thread_func, this, loop_rate);
    // listen for TCP messages from GUI
    Packet packet;
    while (m_running) {
        packet;
        auto status = tcp.receive(packet);
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
                // send logs
                packet << remote_writer.logs.size();
                for (int i = 0; i < remote_writer.logs.size(); ++i)
                    packet << (int)remote_writer.logs[i].first << remote_writer.logs[i].second;
                tcp.send(packet);
                remote_writer.logs.clear();
            }
            else if (msg == Message::Enable) {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_status.enabled = true;
                LOG(Info) << "Enabling pendulum.";
            }
            else if (msg == Message::Disable) {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_status.enabled = false;
                LOG(Info) << "Disabling pendulum.";
            }
            else if (msg == Message::Feedback) {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_status.mode = m_status.mode == (int)Mode::Encoder ? (int)Mode::Midori : (int)Mode::Encoder;
                LOG(Info) << "Changing pendulum feedback mode to " << (m_status.mode == (int)Mode::Encoder ? "Encoder." : "Midori.");
            }
            else if (msg == Message::Zero) {
                std::lock_guard<std::mutex> lock(m_mtx);
                g_zero = true;
                LOG(Info) << "Zeroing pendulum encoder.";
            }
            else if (msg == Message::Shutdown) {
                LOG(Info) << "Shutting down pendulum controller.";
                m_running = false;
            }
        }
        else {
            LOG(Error) << "Unexpected error!";
            m_running = false;
        }
    }
    m_ctrl_thread.join();
    tcp.disconnect();
}

void IPendulum::plot(const std::string& label, double value) {
    if (!m_running) {
        LOG(Warning) << "Plots can only be called when the controller is running!";
        return;
    }
    if (m_plots.size() < 5)
        m_plots.push_back({label,value});
}

void IPendulum::ctrl_thread_func(Frequency loop_rate) {
    LOG(Info) << "Starting pendulum control thread.";
    // initialize UDP stream
    UdpSocket udp;
    auto result = udp.bind(SERVER_UDP);
    if (result == Socket::Done)
        LOG(Info) << "Opened UPD socket on port " << udp.get_local_port() << ".";
    else
        LOG(Error) << "Failed to open UDP socket on port " << udp.get_local_port() << ".";
    State state;
    Packet packet;
    Data data;
    m_plots.reserve(10);
    // initialize myRIO       
    MyRio myrio;
    myrio.mspC.encoder.set_channels({0});
    myrio.mspC.encoder.units[0] = 2 * PI / 500.0;
    myrio.mspC.encoder.zero(0);
    myrio.mspC.DO.set_channels({1});
    myrio.enable();
    // timing
    Timer timer(loop_rate);
    RateMonitor monitor;
    // start the control loop
    while (m_running) {
        Mode mode;
        char enabled;
        // update status
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_status.running   = m_running;
            m_status.frequency = monitor.rate();
            m_status.misses    = (int)timer.get_misses();
            m_status.wait      = timer.get_wait_ratio();
            mode               = (Mode)m_status.mode;
            enabled            = m_status.enabled;
        }
        // check for encoder zero
        if (g_zero) {
            std::lock_guard<std::mutex> lock(m_mtx);
            myrio.mspC.encoder.zero(0);
            g_zero = false;
        }
        if (myrio.mspC.encoder.has_encoder_error({0})){
            myrio.mspC.encoder.clear_encoder_error({0});
            LOG(Verbose) << "Clearing encoder error";
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
            state.command = control_encoder(state.time, state.encoder);
        else if (mode == Mode::Midori)
            state.command = control_midori(state.time, state.midori);
        myrio.mspC.AO[0] = enabled ? state.command : 0;
        myrio.mspC.DO[1] = enabled;
        for (int l = 0; l < 4; ++l)
            myrio.LED[l] = enabled;
        myrio.write_all();
        // stream data
        packet.clear();
        data.state = state;
        data.plots = m_plots;
        packet << data;
        udp.send(packet, CLIENT_IP, CLIENT_UDP);     
        m_plots.clear();         
        if (g_stop)
            m_running = false;
        monitor.tick();
        monitor.update(timer.get_elapsed_time());
        timer.wait();
    }   
    myrio.mspC.AO[0] = 0;
    myrio.mspC.DO[1] = TTL_LOW;
    for (int l = 0; l < 4; ++l)
        myrio.LED[l] = TTL_LOW;
    myrio.write_all();
    state.tick = -1;
    data.state = state;
    data.plots.clear();
    packet.clear();
    packet << data;
    udp.send(packet, CLIENT_IP, CLIENT_UDP);
    myrio.disable();
    myrio.close();
    LOG(Info) << "Terminated pendulum control thread.";
}