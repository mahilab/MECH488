#pragma once
#include <Mahi/Com.hpp>
#include <Mahi/Util.hpp>
#include <utility>

using namespace mahi::com;
using namespace mahi::util;

#define SERVER_IP "172.22.11.2" // myRIO IP address over USB LAN
#define CLIENT_IP "172.22.11.1" // Windows IP address over USB LAN

#define SERVER_TCP 55001        // myRIO TCP port
#define SERVER_UDP 55002        // myRIO UDP port
#define CLIENT_UDP 55003        // Windows UDP port

enum Message {
    Ping       = 0,
    Enable     = 1,
    Disable    = 2,
    Feedback   = 3,
    Zero       = 4,
    Shutdown   = 5
};

enum Mode {
    Encoder = 0,
    Midori  = 1
};

struct Status {
    bool   running   = false;
    bool   enabled   = false;
    int    mode      = Mode::Encoder;
    double frequency = 0;
    int    misses    = 0;
    double wait      = 0;
};

inline Packet& operator<<(Packet& packet, const Status& status) {
    return packet << status.running << status.enabled << status.mode << status.frequency << status.misses << status.wait;
}

inline Packet& operator>>(Packet& packet, Status& status) {
    return packet >> status.running >> status.enabled >> status.mode >> status.frequency >> status.misses >> status.wait;
}

struct State {
    int    tick;    // [0...N]
    double time;    // [s]
    double sense;   // [V]
    double command; // [V]
    double midori;  // [V]
    int    encoder; // [counts]
    char   enable;  // [0/1]
};

inline Packet& operator<<(Packet& packet, const State& state) {
    return packet << state.tick << state.time << state.sense << state.command << state.midori << state.encoder << state.enable;
}

inline Packet& operator>>(Packet& packet, State& state) {
    return packet >> state.tick >> state.time >> state.sense >> state.command >> state.midori >> state.encoder >> state.enable;
}

typedef RingBuffer<std::pair<Severity, std::string>> LogBuffer;

struct Plot {
    std::string label;
    double      value;
};

inline Packet& operator<<(Packet& packet, const Plot& plot) {
    return packet << plot.label << plot.value;
}

inline Packet& operator>>(Packet& packet, Plot& plot) {
    return packet >> plot.label >> plot.value;
}

struct Data {
    State state;
    std::vector<Plot> plots;
};

inline Packet& operator<<(Packet& packet, const Data& data) {
    packet << data.state << (int)data.plots.size();
    for (auto& p : data.plots)
        packet << p;
    return packet;
};

inline Packet& operator>>(Packet& packet, Data& data) {
    int plots_size;
    packet >> data.state >> plots_size;
    data.plots.resize(plots_size);
    for (auto& p : data.plots)
        packet >> p;
    return packet;
};