#pragma once
#include <Mahi/Com.hpp>
#include <Mahi/Util.hpp>

using namespace mahi::com;
using namespace mahi::util;

#define SERVER_IP "172.22.11.2" // myRIO IP address over USB LAN
#define CLIENT_IP "172.22.11.1" // Windows IP address over USB LAN

#define SERVER_TCP 55001        // myRIO TCP port
#define SERVER_UDP 55002        // myRIO UDP port
#define CLIENT_UDP 55003        // Windows UDP port

/// Typedef this monstrosity so we don't have to type it out again.
typedef RingBuffer<std::pair<Severity, std::string>> LogBuffer;

/// Types of messages the GUI may send to the myRIO pendulum.
enum Message {
    Ping       = 0,
    Enable     = 1,
    Disable    = 2,
    Feedback   = 3,
    Zero       = 4,
    Shutdown   = 5
};

/// The feedback modes the myRIO pendulum can be in.
enum Mode {
    Encoder = 0,
    Midori  = 1
};

/// Status information for the myRIO pendulum controller.
struct Status {
    bool   running   = false;         ///< is the controller loop running?
    bool   enabled   = false;         ///< is the pendulum amplifier enabled?
    int    mode      = Mode::Encoder; ///< which feedback mode are we in?
    double frequency = 0;             ///< the actual loop rate in Hz
    int    misses    = 0;             ///< the number of times our controller loop has missed its deadline
    double wait      = 0;             ///< the percentage of time we spend waiting for the next loop
};

/// Serialize Status to Packet.
inline Packet& operator<<(Packet& packet, const Status& status) {
    return packet << status.running << status.enabled << status.mode << status.frequency << status.misses << status.wait;
}

/// Deserialize Packet to Status.
inline Packet& operator>>(Packet& packet, Status& status) {
    return packet >> status.running >> status.enabled >> status.mode >> status.frequency >> status.misses >> status.wait;
}

/// State of the myRIO pendulum controller.
struct State {
    int    tick;    ///< the controller tick number    [0...N]
    double time;    ///< the controller time           [s]
    double sense;   ///< the amplifier sense voltage   [V]
    double command; ///< the amplifier command voltage [V]
    double midori;  ///< the Midori pot voltage        [V]
    int    encoder; ///< the encoder counts            [counts]
    char   enable;  ///< the amplifier enable state    [0=disabled,1=enabled]
};

/// Serialize State to Packet.
inline Packet& operator<<(Packet& packet, const State& state) {
    return packet << state.tick << state.time << state.sense << state.command << state.midori << state.encoder << state.enable;
}

/// Deserialize Packet to State.
inline Packet& operator>>(Packet& packet, State& state) {
    return packet >> state.tick >> state.time >> state.sense >> state.command >> state.midori >> state.encoder >> state.enable;
}

/// User defined plot value.
struct Plot {
    std::string label;
    double      value;
};

/// Serialize Plot to Packet.
inline Packet& operator<<(Packet& packet, const Plot& plot) {
    return packet << plot.label << plot.value;
}

/// Deserialize Packet to Plot.
inline Packet& operator>>(Packet& packet, Plot& plot) {
    return packet >> plot.label >> plot.value;
}

/// Merger of the controller state and all user plots for each loop tick.
struct Data {
    State state;
    std::vector<Plot> plots;
};

/// Serialize Data to Packet.
inline Packet& operator<<(Packet& packet, const Data& data) {
    packet << data.state << (int)data.plots.size();
    for (auto& p : data.plots)
        packet << p;
    return packet;
};

/// Deserialize Packet to Data.
inline Packet& operator>>(Packet& packet, Data& data) {
    int plots_size;
    packet >> data.state >> plots_size;
    data.plots.resize(plots_size);
    for (auto& p : data.plots)
        packet >> p;
    return packet;
};