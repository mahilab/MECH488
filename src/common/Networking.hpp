#pragma once
#include <Mahi/Com.hpp>

using namespace mahi::com;

#define SERVER_IP "172.22.11.2" // myRIO IP address over USB LAN
#define CLIENT_IP "172.22.11.1" // Windows IP address over USB LAN

#define SERVER_TCP 55001        // myRIO TCP port
#define SERVER_UDP 55002        // myRIO UDP port
#define CLIENT_UDP 55003        // Windows UDP port

enum Message {
    Ping = 0,
    Enable = 1,
    Disable = 2,
    ChangeMode = 3,
    Shutdown = 4
};

enum Mode {
    Encoder = 0,
    Midori  = 1
};

struct Status {
    bool   running   = false;
    bool   enabled   = false;
    int    mode      = Mode::Encoder;
    int    misses    = 0;
};

inline Packet& operator<<(Packet& packet, const Status& status) {
    return packet << status.running << status.enabled << status.mode << status.misses;
}

inline Packet& operator>>(Packet& packet, Status& status) {
    return packet >> status.running >> status.enabled >> status.mode >> status.misses;
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