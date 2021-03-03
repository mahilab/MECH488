#include "PendulumGui.hpp"

template <class Formatter>
class GuiLogWritter : public Writer {
public:
    GuiLogWritter(Severity max_severity = Debug) : Writer(max_severity), l_logs(500), r_logs(500) {}

    virtual void write(const LogRecord& record) override {
        auto log = std::pair<Severity, std::string>(record.get_severity(), Formatter::format(record));
        l_logs.push_back(log);
    }
    LogBuffer l_logs;
    LogBuffer r_logs;
};

static GuiLogWritter<TxtFormatter> writer;

//=============================================================================

#define WIDTH 1260
#define HEIGHT 720
#ifdef _DEBUG
#define TITLE "Pendulum GUI - MAHI Lab (Debug)"
#else
#define TITLE "Pendulum GUI - MAHI Lab"
#endif

PendulumGui::PendulumGui() : 
    Application(WIDTH,HEIGHT,TITLE,false),
    m_connected(false),
    m_queue(2000)
{
    style_gui();
    if (MahiLogger) {
        MahiLogger->add_writer(&writer);
        MahiLogger->set_max_severity(Debug);
    }
    auto result = m_udp.bind(CLIENT_UDP);
    if (result == Socket::Done)
        LOG(Info) << "Opened UPD socket on port " << m_udp.get_local_port() << ".";
    else
        LOG(Error) << "Failed to open UDP socket on port " << m_udp.get_local_port() << ".";
    ImGui::DisableViewports();
    ImGui::DisableDocking();
}

PendulumGui::~PendulumGui() {
    m_connected = false;
}

void PendulumGui::update() {

    ping();

    constexpr int pad     = 10;
    constexpr int w_left = 250;
    constexpr int h_comm = 190;
    constexpr int h_stat = 175;
    constexpr int h_netw = 175;
    constexpr int w_logs = WIDTH/2-pad-pad/2;
    constexpr int h_logs = HEIGHT - 5*pad - h_comm - h_stat - h_netw;

    ImGui::BeginFixed("Commands", ImVec2(pad,pad), ImVec2(w_left,h_comm), ImGuiWindowFlags_NoCollapse);
    show_cmds();
    ImGui::End();

    ImGui::BeginFixed("Controller Status", ImVec2(pad,h_comm+2*pad), ImVec2(w_left,h_stat), ImGuiWindowFlags_NoCollapse);
    show_status();
    ImGui::End();

    ImGui::BeginFixed("Network Status", ImVec2(pad,h_comm+h_stat+3*pad), ImVec2(w_left, h_netw), ImGuiWindowFlags_NoCollapse);
    show_network();
    ImGui::End();

    static ImGuiTextFilter l_filter;
    static bool            l_verb = false;
    ImGui::BeginFixed("Local Logs", ImVec2(pad,h_comm+h_stat+h_netw+4*pad), ImVec2(w_logs,h_logs), ImGuiWindowFlags_NoCollapse);
    show_logs(writer.l_logs, l_filter, l_verb);
    ImGui::End();

    static ImGuiTextFilter r_filter;
    static bool            r_verb = false;
    ImGui::BeginFixed("Remote Logs", ImVec2(2*pad+w_logs,h_comm+h_stat+h_netw+4*pad), ImVec2(w_logs,h_logs), ImGuiWindowFlags_NoCollapse);
    show_logs(writer.r_logs,r_filter,r_verb);
    ImGui::End();

    ImGui::BeginFixed("Data", ImVec2(w_left+2*pad,pad), ImVec2(WIDTH-3*pad-w_left,h_comm+h_stat+h_netw+2*pad), ImGuiWindowFlags_NoCollapse);
    show_plot();
    ImGui::End();

}

bool PendulumGui::connect() {
    auto status = m_tcp.connect(SERVER_IP, SERVER_TCP, seconds(0.1));
    if (status == Socket::Status::Done) {
        LOG(Info) << "Connected to myRIO: " << m_tcp.get_remote_port() << "@" << m_tcp.get_remote_address();
        clear_data();
        m_connected = true;
        m_data_thread = std::thread(&PendulumGui::data_thread_func, this);
        m_data_thread.detach();
        m_msgSent = 0;
        return true;
    }
    else {
        LOG(Error) << "Failed to connect to myRIO. Ensure that the Pendulum application is running on the myRIO.";
        m_connected = false;
        return false;
    }
}

bool PendulumGui::ping() {
    if (m_connected && send_message(Message::Ping)) {
        Packet packet;
        auto result = m_tcp.receive(packet);
        if (result == Socket::Done) {
            int new_logs;
            packet >> m_status >> new_logs;
            for (int i = 0; i < new_logs; ++i) {
                int sev;
                std::string msg;
                packet >> sev >> msg;
                auto log = std::pair<Severity, std::string>((Severity)sev, msg);
                writer.r_logs.push_back(log);
            }

            m_connected = true;
            return true;
        }
        else {
            LOG(Warning) << "Lost connection to myRIO.";
            m_connected = false;
            return false;
        }
    }
    return false;
}

bool PendulumGui::send_message(Message msg) {
    if (m_connected) {
        Packet packet;
        packet << (int)msg;
        auto result = m_tcp.send(packet);
        if (result == Socket::Done) {
            m_msgSent++;
            return true;
        }
        else {
            LOG(Error) << "Lost connection to myRIO.";
            m_connected = false;
            return false;
        }
    }
    return false;
}

void PendulumGui::data_thread_func() {
    LOG(Info) << "Starting data streaming thread.";
    Packet packet;
    Data data;
    unsigned short port;
    IpAddress address;
    int lastTick = -1;
    m_packsLost = 0;
    m_packsRecv = 0;
    bool keep_alive = true;
    while (m_connected && keep_alive) {
        packet.clear();
        auto result = m_udp.receive(packet, address, port);
        if (result == Socket::Done) {
            packet >> data;
            if (data.state.tick == -1) {
                keep_alive = false; 
                break;
            } 
            else if (lastTick + 1 != data.state.tick) 
                m_packsLost++;         
            m_queue.try_push(data);
            m_packsRecv++;
            lastTick = data.state.tick;
        }
    }
    LOG(Info) << "Terminated data streaming thread.";
}

void PendulumGui::clear_data() {
    m_timeData.clear();
    m_senseData.clear();
    m_commandData.clear();
    m_midoriData.clear();
    m_encoderData.clear();
    m_enableData.clear();
    m_plots.clear();
}

void PendulumGui::export_data(const std::string& filepath) {
    std::ofstream file;
    file.open(filepath);
    if (!file.is_open())
    {
        LOG(Error) << "Failed to open file " << filepath << ". Is it open in another application?";
        return;
    }

    std::lock_guard<std::mutex> lock(m_data_mtx);
    // write header
    file << "Time [s],Sense [V],Command [V],Midori [V],Encoder [counts],Enable,";
    for (auto& p : m_plots) 
        file << p.first << ",";
    file << std::endl;
    // write data
    int i = m_timeData.offset;
    int N = m_timeData.size;
    for (int n = 0; n < N; ++n) {
        file << m_timeData.data[i]    << ","
             << m_senseData.data[i]   << ","
             << m_commandData.data[i] << ","
             << m_midoriData.data[i]  << ","
             << m_encoderData.data[i] << ","
             << m_enableData.data[i]  << ",";
        for (auto& p : m_plots)
            file << p.second.data[i] << ",";
        file << std::endl;
        if (++i == N)
            i = 0;             
    }
    file.close();
    LOG(Info) << "Exported data to " << filepath << ".";
}

void PendulumGui::show_cmds() {
    ImGui::BeginDisabled(m_connected);
    if (ImGui::Button("Connect", ImVec2(-1,0))) 
        connect();    
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!m_connected || m_status.enabled);
    if (ImGui::Button("Enable", ImVec2(-1,0))) 
        send_message(Message::Enable);    
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!m_connected || !m_status.enabled);
    if (ImGui::Button("Disable", ImVec2(-1,0))) 
        send_message(Message::Disable);   
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!m_connected);
    if (ImGui::Button("Change Feedback", ImVec2(-1,0))) 
        send_message(Message::Feedback);    
    if (ImGui::Button("Zero Encoder", ImVec2(-1,0)))
        send_message(Message::Zero);
    if (ImGui::Button("Shutdown", ImVec2(-1,0))) {
        send_message(Message::Shutdown); 
        m_status = Status();
    }
    ImGui::EndDisabled();
}

void info_line(const char* left, const char* right, Color col =  ImGui::GetStyleColorVec4(ImGuiCol_Text)) {
    ImGui::TextUnformatted(left);
    ImGui::SameLine(120);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(right);
    ImGui::PopStyleColor();
}

void PendulumGui::show_status() {
    if (m_connected) {
        info_line("Connected", m_connected ? "True" : "False", m_connected ? Blues::DeepSkyBlue : ImVec4(0.951f, 0.208f, 0.387f, 1.000f));
        info_line("Running", m_status.running ? "True" : "False", m_status.running ? Blues::DeepSkyBlue : ImVec4(0.951f, 0.208f, 0.387f, 1.000f));
        info_line("Enabled", m_status.enabled ? "True" : "False", m_status.enabled ? Blues::DeepSkyBlue : ImVec4(0.951f, 0.208f, 0.387f, 1.000f));
        info_line("Feedback", m_status.mode == (int)Mode::Encoder ? "Enocder" : "Midori");
        info_line("Loop Rate", fmt::format("{} Hz",m_status.frequency).c_str());
        info_line("Misses", fmt::format("{}",m_status.misses).c_str());
        info_line("Wait Ratio", fmt::format("{:.1f}%",m_status.wait*100).c_str());
    }
    else {
        ImGui::Text("Connect myRIO");
    }
}

void PendulumGui::show_network() {
    if (m_connected) {
        auto defcol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        info_line("TCP Local", fmt::format("{}@{}",m_tcp.get_local_port(), CLIENT_IP).c_str());
        info_line("TCP Remote", fmt::format("{}@{}",m_tcp.get_remote_port(), SERVER_IP).c_str());
        info_line("UDP Local", fmt::format("{}@{}",CLIENT_UDP, CLIENT_IP).c_str());
        info_line("UDP Remote", fmt::format("{}@{}",SERVER_UDP, SERVER_IP).c_str());
        info_line("Sent",fmt::format("{}", m_msgSent).c_str());
        info_line("Received",fmt::format("{}", m_packsRecv).c_str());
        info_line("Lost",fmt::format("{}", m_packsLost).c_str());
        // ImGui::LabelText("Local", "%d@%s", CLIENT_UDP, CLIENT_IP);
        // ImGui::LabelText("Remote", "%d@%s", SERVER_UDP, SERVER_IP);

        // ImGui::LabelText("Local", "%d@%s", m_tcp.get_local_port(), CLIENT_IP);
        // ImGui::LabelText("Remote", "%d@%s", m_tcp.get_remote_port(), m_tcp.get_remote_address().to_string());
        // ImGui::Separator();
        // ImGui::LabelText("Local", "%d@%s", CLIENT_UDP, CLIENT_IP);
        // ImGui::LabelText("Remote", "%d@%s", SERVER_UDP, SERVER_IP);
        // ImGui::LabelText("Packets", "%d / %d", m_packsRecv, m_packsLost);
    }
    else {
        ImGui::Text("Connect myRIO");
    }
}

void PendulumGui::show_plot() {

    static double latestTime = 0;
    static bool   paused     = false;
    static std::set<std::string> seen;
    seen.clear();

    // thread safe section
    {        
        std::lock_guard<std::mutex> lock(m_data_mtx);
        // pop samples of queue
        while (m_queue.front()) {
            auto data = *m_queue.front();
            m_queue.pop();
            int size   = m_timeData.size;
            int offset = m_timeData.offset;
            latestTime = data.state.time;
            if (!paused) {
                m_timeData.push_back(latestTime);
                m_senseData.push_back(data.state.sense);
                m_commandData.push_back(data.state.command);
                m_midoriData.push_back(data.state.midori);
                m_encoderData.push_back(data.state.encoder);
                m_enableData.push_back(data.state.enable);
            }
            // user plots
            for (auto& p : data.plots) {
                // flag as having been seen
                seen.insert(p.label);
                if (!paused) {
                    m_plots[p.label].size   = size;
                    m_plots[p.label].offset = offset;
                    m_plots[p.label].push_back(p.value);
                }
            }
            // pad unseen plots
            if (!paused) {
                for (auto& p : m_plots) {
                    if (!seen.count(p.first))
                        p.second.push_back(0);
                }
            }             
        }
    }

    if (ImGui::Button("Clear",ImVec2(100,0))) {
        clear_data();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export",ImVec2(100,0))) {
        paused = true;
        auto sd = [this]() {
            std::string path;
            if (save_dialog(path, {{"CSV","csv"}}) == DialogResult::DialogOkay)
                export_data(path);
        };
        std::thread thrd(sd);
        thrd.detach();
    }
    ImGui::SameLine();
    if (ImGui::Button(paused ? "Resume" : "Pause",ImVec2(100,0))) {
        if (paused)
            clear_data();
        paused = !paused;
    }
    static bool show_default = true;
    ImGui::SameLine();
    ImGui::Checkbox("Default Plots",&show_default);

    ImGui::SameLine(880);
    ImGui::Text("    %.3f FPS", ImGui::GetIO().Framerate);
    if (!paused && m_connected)
        ImPlot::SetNextPlotLimitsX(latestTime - 10, latestTime, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(-10,10, ImGuiCond_Appearing, ImPlotYAxis_2);
    ImPlot::SetNextPlotLimitsY(-2000,2000,ImGuiCond_Appearing, ImPlotYAxis_3);
    if (ImPlot::BeginPlot("##State", "Time [s]", NULL, ImVec2(-1,-1), show_default ? ImPlotFlags_YAxis2 | ImPlotFlags_YAxis3 : 0, 0, 0, 0, 0, "Voltage [V]", "Counts")) {
        if (show_default && m_timeData.size > 0) {
            ImPlot::SetPlotYAxis(ImPlotYAxis_2);
            ImPlot::SetNextFillStyle(Blues::DeepSkyBlue);        
            ImPlot::PlotDigital("Enable",  &m_timeData.data[0], &m_enableData.data[0], m_timeData.size, m_timeData.offset);
            ImPlot::SetNextLineStyle(Yellows::Yellow);
            ImPlot::PlotLine("Sense", &m_timeData.data[0], &m_senseData.data[0], m_timeData.size, m_timeData.offset);
            ImPlot::SetNextLineStyle(Oranges::Orange);
            ImPlot::PlotLine("Command", &m_timeData.data[0], &m_commandData.data[0], m_timeData.size, m_timeData.offset);
            ImPlot::SetNextLineStyle(Cyans::LightSeaGreen);
            ImPlot::PlotLine("Midori", &m_timeData.data[0], &m_midoriData.data[0], m_timeData.size, m_timeData.offset);
            ImPlot::SetPlotYAxis(ImPlotYAxis_3);
            ImPlot::SetNextLineStyle(Whites::White);
            ImPlot::PlotLine("Encoder", &m_timeData.data[0], &m_encoderData.data[0], m_timeData.size, m_timeData.offset);
        }
        if (m_timeData.size > 0) {
            ImPlot::SetPlotYAxis(ImPlotYAxis_1);
            for (auto& p : m_plots) {
                if (seen.count(p.first))
                    ImPlot::PlotLine(p.first.c_str(), &m_timeData.data[0], &p.second.data[0], m_timeData.size, m_timeData.offset);
            }
        }
        ImPlot::EndPlot();
    }
}

void PendulumGui::show_logs(LogBuffer& logs, ImGuiTextFilter& filter, bool& verb) {
    static std::unordered_map<Severity, Color> colors = {
        {None, Grays::Gray50},      {Fatal, Reds::Red}, {Error, ImVec4(0.951f, 0.208f, 0.387f, 1.000f)},
        {Warning, Yellows::Yellow}, {Info, ImGui::GetStyleColorVec4(ImGuiCol_Text)},  {Verbose, Cyans::LightSeaGreen},
        {Debug, Cyans::Cyan}};

    if (ImGui::Button("Clear"))
        logs.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Show All",&verb);
    ImGui::SameLine(200);
    filter.Draw("Filter", -40);
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < logs.size(); ++i) {
        if (filter.PassFilter(logs[i].second.c_str()) && (verb ? true : logs[i].first < Severity::Verbose)) {
            ImGui::PushStyleColor(ImGuiCol_Text, colors[logs[i].first]);
            ImGui::TextUnformatted(logs[i].second.c_str());
            ImGui::PopStyleColor();
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

void PendulumGui::style_gui() {

    ImVec4 dark_accent  = ImVec4(0.951f, 0.268f, 0.208f, 1.000f);
    ImVec4 light_accent = ImVec4(0.951f, 0.208f, 0.387f, 1.000f);

    set_background({0.15f, 0.16f, 0.21f, 1.00f});
    // ImPlot::SetColormap(ImPlotColormap_Default);

    auto& style = ImGui::GetStyle();
    style.WindowPadding = {6,6};
    style.FramePadding  = {6,3};
    style.CellPadding   = {6,3};
    style.ItemSpacing   = {6,6};
    style.ItemInnerSpacing = {6,6};
    style.ScrollbarSize = 16;
    style.GrabMinSize = 8;
    style.WindowBorderSize = style.ChildBorderSize = style.PopupBorderSize = style.TabBorderSize = 0;
    style.FrameBorderSize = 1;
    style.WindowRounding = style.ChildRounding = style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 4;

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.89f, 0.89f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.20f, 0.21f, 0.27f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.06f);
    colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_FrameBgHovered]         = light_accent;
    colors[ImGuiCol_FrameBgActive]          = light_accent;
    colors[ImGuiCol_TitleBg]                = dark_accent;
    colors[ImGuiCol_TitleBgActive]          = dark_accent;
    colors[ImGuiCol_TitleBgCollapsed]       = dark_accent;
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.89f, 0.89f, 0.93f, 0.27f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.89f, 0.89f, 0.93f, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    colors[ImGuiCol_CheckMark]              = dark_accent;
    colors[ImGuiCol_SliderGrab]             = dark_accent;
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    colors[ImGuiCol_Button]                 = dark_accent;
    colors[ImGuiCol_ButtonHovered]          = light_accent;
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_Header]                 = dark_accent;
    colors[ImGuiCol_HeaderHovered]          = light_accent;
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_Separator]              = dark_accent;
    colors[ImGuiCol_SeparatorHovered]       = light_accent;
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = dark_accent;
    colors[ImGuiCol_ResizeGripHovered]      = light_accent;
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TabHovered]             = light_accent;
    colors[ImGuiCol_TabActive]              = dark_accent;
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_PlotLines]              = light_accent;
    colors[ImGuiCol_PlotLinesHovered]       = light_accent;
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

    ImVec4* pcolors = ImPlot::GetStyle().Colors;
    pcolors[ImPlotCol_Line]          = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_Fill]          = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_MarkerOutline] = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_MarkerFill]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_ErrorBar]      = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_FrameBg]       = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_PlotBg]        = ImVec4(0.07f, 0.07f, 0.10f, 0.00f);
    pcolors[ImPlotCol_PlotBorder]    = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    pcolors[ImPlotCol_LegendBg]      = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_LegendBorder]  = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_LegendText]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_TitleText]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_InlayText]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_XAxis]         = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_XAxisGrid]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis]         = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis2]        = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid2]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis3]        = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid3]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_Selection]     = ImVec4(0.000f, 0.571f, 1.000f, 1.000f);
    pcolors[ImPlotCol_Query]         = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_Crosshairs]    = IMPLOT_AUTO_COL;

    ImPlot::GetStyle().DigitalBitHeight = 20;

    auto& pstyle = ImPlot::GetStyle();
    pstyle.PlotPadding = pstyle.LegendPadding = {12,12};
    pstyle.LabelPadding = pstyle.LegendInnerPadding = {6,6};
    pstyle.FitPadding   = {0,0.1f};
    pstyle.LegendSpacing = {2,2};

    auto& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig font_cfg;
    font_cfg.PixelSnapH           = true;
    font_cfg.OversampleH          = 1;
    font_cfg.OversampleV          = 1;
    font_cfg.FontDataOwnedByAtlas = false;
    strcpy(font_cfg.Name, "Roboto Bold");
    io.Fonts->AddFontFromMemoryTTF(Roboto_Bold_ttf, Roboto_Bold_ttf_len, 15.0f, &font_cfg);
}