#include "PendulumGui.hpp"

template <class Formatter>
class GuiLogWritter : public Writer {
public:
    GuiLogWritter(Severity max_severity = Debug) : Writer(max_severity), logs(500) {}

    virtual void write(const LogRecord& record) override {
        auto log =
            std::pair<Severity, std::string>(record.get_severity(), Formatter::format(record));
        logs.push_back(log);
    }
    RingBuffer<std::pair<Severity, std::string>> logs;
};

static GuiLogWritter<TxtFormatter> writer;

//=============================================================================

PendulumGui::PendulumGui() : 
    Application(1260,720,"Pendulum GUI"),
    m_connected(false),
    m_states(2000)
{
    style_gui();
    if (MahiLogger) {
        MahiLogger->add_writer(&writer);
        MahiLogger->set_max_severity(Debug);
    }
    auto result = m_udp.bind(CLIENT_UDP);
    if (result == Socket::Done)
        LOG(Info) << "UPD socket bound to port " << m_udp.get_local_port();
    else
        LOG(Error) << "Failed to bind UDP socket to port " << m_udp.get_local_port();
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
    constexpr int h_comm = 165;
    constexpr int h_stat = 112;
    constexpr int h_netw = 265;
    constexpr int h_logs = 720 - 5*pad - h_comm - h_stat - h_netw;

    ImGui::BeginFixed("Commands", ImVec2(pad,pad), ImVec2(w_left,h_comm), ImGuiWindowFlags_NoCollapse);
    show_cmds();
    ImGui::End();

    ImGui::BeginFixed("Status", ImVec2(pad,h_comm+2*pad), ImVec2(w_left,h_stat), ImGuiWindowFlags_NoCollapse);
    show_status();
    ImGui::End();

    ImGui::BeginFixed("Network", ImVec2(pad,h_comm+h_stat+3*pad), ImVec2(w_left, h_netw), ImGuiWindowFlags_NoCollapse);
    show_network();
    ImGui::End();

    ImGui::BeginFixed("Logs", ImVec2(pad,h_comm+h_stat+h_netw+4*pad), ImVec2(1260-2*pad,h_logs), ImGuiWindowFlags_NoCollapse);
    show_logs();
    ImGui::End();


    ImGui::BeginFixed("Plot", ImVec2(w_left+2*pad,pad), ImVec2(1260-3*pad-w_left,h_comm+h_stat+h_netw+2*pad), ImGuiWindowFlags_NoCollapse);
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
            packet >> m_status;
            m_connected = true;
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
    LOG(Info) << "Starting data stream thread.";
    Packet packet;
    State state;
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
            packet >> state;
            if (state.tick == -1) {
                keep_alive = false; 
                break;
            } 
            else if (lastTick + 1 != state.tick) 
                m_packsLost++;         
            if (!m_states.try_push(state))
                m_packsLost++;
            m_packsRecv++;
            lastTick = state.tick;
        }
    }
    LOG(Info) << "Data stream thread terminated.";
}

void PendulumGui::clear_data() {
    m_timeData.clear();
    m_senseData.clear();
    m_commandData.clear();
    m_midoriData.clear();
    m_encoderData.clear();
    m_enableData.clear();
}

void PendulumGui::show_network() {
    if (m_connected) {
        ImGui::Text("TCP Messaging");
        ImGui::LabelText("Local", "%d@%s", m_tcp.get_local_port(), CLIENT_IP);
        ImGui::LabelText("Remote", "%d@%s", m_tcp.get_remote_port(), m_tcp.get_remote_address().to_string());
        ImGui::LabelText("Sent", "%d", m_msgSent);
        ImGui::Separator();
        ImGui::Text("UDP Data Stream");
        ImGui::LabelText("Local", "%d@%s", CLIENT_UDP, CLIENT_IP);
        ImGui::LabelText("Remote", "%d@%s", SERVER_UDP, SERVER_IP);
        ImGui::LabelText("Received", "%d", m_packsRecv);
        ImGui::LabelText("Lost", "%d", m_packsLost);
    }
    else {
        ImGui::Text("Connect myRIO");
    }
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
    if (ImGui::Button("Change Mode", ImVec2(-1,0)))
        send_message(Message::ChangeMode);
    if (ImGui::Button("Shutdown", ImVec2(-1,0))) {
        send_message(Message::Shutdown); 
        m_status = Status();
    }
    ImGui::EndDisabled();
}

void status_line(const char* left, const char* right, Color col) {
    ImGui::TextUnformatted(left);
    ImGui::SameLine(150);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(right);
    ImGui::PopStyleColor();
}

void PendulumGui::show_status() {
    if (m_connected) {
        status_line("Connected:", m_connected ? "True" : "False", m_connected ? Blues::DeepSkyBlue : ImVec4(0.85f, 0.37f, 0.00f, 1.00f));
        status_line("Running:", m_status.running ? "True" : "False", m_status.running ? Blues::DeepSkyBlue : ImVec4(0.85f, 0.37f, 0.00f, 1.00f));
        status_line("Enabled:", m_status.enabled ? "True" : "False", m_status.enabled ? Blues::DeepSkyBlue : ImVec4(0.85f, 0.37f, 0.00f, 1.00f));
        status_line("Mode:", m_status.mode == (int)Mode::Encoder ? "Enocder" : "Midori", ImGui::GetStyleColorVec4(ImGuiCol_Text));
    }
    else {
        ImGui::Text("Connect myRIO");
    }
}

void PendulumGui::show_logs() {
    // log window
    static ImGuiTextFilter                     filter;
    static std::unordered_map<Severity, Color> colors = {
        {None, Grays::Gray50},      {Fatal, Reds::Red}, {Error, Pinks::HotPink},
        {Warning, Yellows::Yellow}, {Info, Whites::White},  {Verbose, Greens::Chartreuse},
        {Debug, Cyans::Cyan}};

    // if (ImGui::Button("Clear"))
    //     writer.logs.clear();
    // ImGui::SameLine();
    // filter.Draw("Filter", -50);
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < writer.logs.size(); ++i) {
        if (filter.PassFilter(writer.logs[i].second.c_str())) {
            ImGui::PushStyleColor(ImGuiCol_Text, colors[writer.logs[i].first]);
            ImGui::TextUnformatted(writer.logs[i].second.c_str());
            ImGui::PopStyleColor();
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

void PendulumGui::show_plot() {

    static double latestTime = 0;
    static bool   paused     = false;

    while (m_states.front()) {
        auto state = *m_states.front();
        m_states.pop();
        if (!paused) {
            latestTime = state.time;
            m_timeData.push_back(state.time);
            m_senseData.push_back(state.sense);
            m_commandData.push_back(state.command);
            m_midoriData.push_back(state.midori);
            m_encoderData.push_back(state.encoder);
            m_enableData.push_back(state.enable);
        }
    }

    if (ImGui::Button("Clear",ImVec2(100,0))) {
        clear_data();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export",ImVec2(100,0))) {

    }
    ImGui::SameLine();
    if (ImGui::Button(paused ? "Resume" : "Pause",ImVec2(100,0))) {
        if (paused)
            clear_data();
        paused = !paused;
    }
    ImGui::SameLine(880);
    ImGui::Text("    %.3f FPS", ImGui::GetIO().Framerate);
    if (!paused)
        ImPlot::SetNextPlotLimitsX(latestTime - 10, latestTime, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(-10,10,ImPlotYAxis_1);
    if (ImPlot::BeginPlot("##State", "Time [s]", "Voltage [V]", ImVec2(-1,-1), ImPlotFlags_YAxis2, 0, 0, 0, 0, "Counts")) {
        ImPlot::SetNextFillStyle(Blues::DeepSkyBlue);        
        ImPlot::PlotDigital("Enable",  &m_timeData.data[0], &m_enableData.data[0], m_timeData.data.size(), m_timeData.offset);
        ImPlot::PlotLine("Sense [V]", &m_timeData.data[0], &m_senseData.data[0], m_timeData.data.size(), m_timeData.offset);
        ImPlot::PlotLine("Command [V]", &m_timeData.data[0], &m_commandData.data[0], m_timeData.data.size(), m_timeData.offset);
        ImPlot::PlotLine("Midori [V]", &m_timeData.data[0], &m_midoriData.data[0], m_timeData.data.size(), m_timeData.offset);
        ImPlot::SetPlotYAxis(ImPlotYAxis_2);
        ImPlot::PlotLine("Encoder [counts]", &m_timeData.data[0], &m_encoderData.data[0], m_timeData.data.size(), m_timeData.offset);
        ImPlot::EndPlot();
    }
}

void PendulumGui::style_gui() {
    ImVec4 dark_accent  = ImVec4(0.85f, 0.37f, 0.00f, 1.00f);
    ImVec4 light_accent = ImVec4(1.00f, 0.63f, 0.00f, 1.00f);

    set_background({0.15f, 0.16f, 0.21f, 1.00f});
    ImPlot::SetColormap(ImPlotColormap_Default);

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