#pragma once
#include <Mahi/Com.hpp>

using namespace mahi::com;

#define SERVER_IP "172.22.11.2" // myRIO IP address over USB LAN
#define CLIENT_IP "172.22.11.1" // Windows IP address over USB LAN

#define SERVER_TCP 55001        // myRIO TCP port
#define SERVER_UDP 55002        // myRIO UDP port
#define CLIENT_UDP 55003        // Windows UDP port

enum Message {
    Ping,
    Enable,
    Disable,
    Shutdown
};

struct Status {
    bool   running   = false;
    bool   enabled   = false;
    int    misses    = 0;
};

inline Packet& operator<<(Packet& packet, const Status& status) {
    return packet << status.running << status.enabled;
}

inline Packet& operator>>(Packet& packet, Status& status) {
    return packet >> status.running >> status.enabled;
}

struct State {
    int    tick;
    double time;
};

inline Packet& operator<<(Packet& packet, const State& state) {
    return packet << state.tick << state.time;
}

inline Packet& operator>>(Packet& packet, State& state) {
    return packet >> state.tick >> state.time;
}
