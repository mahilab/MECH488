#pragma once

#include "common.hpp"     // for types needed to communicate with GUI
#include <Mahi/Robo.hpp>  // for Butterworth
#include <thread>         // for std::thread
#include <mutex>          // for std::mutex
#include <atomic>         // for std::atomic_bool

// so we can say foo() instead of mahi::daq::foo() etc.
using namespace mahi::robo;
using namespace mahi::util;

/// Pendulum interface. Abstract base class.
class IPendulum  {
public:
    /// Constructor.
    IPendulum();
    /// Destructor.
    virtual ~IPendulum();
    /// Run the pendulum interface at a desired loop rate.
    void run(Frequency loop_rate = 1000_Hz);
    /// Plot a value to the pendulum GUI.
    void plot(const std::string& label, double value);
    /// Interface to implement control with encoder position feedback.
    virtual double control_encoder(double t, int counts) = 0;
    /// Interface to implement control with Midori potentiometer position feedback.
    virtual double control_midori(double t, double midori_volts) = 0;
private:
    /// The function that will by run by the control thread.
    void ctrl_thread_func(Frequency loop_rate);
private:
    std::thread       m_ctrl_thread;  // thread that will run the controller
    std::mutex        m_mtx;          // mutex that will protect state shared by control and main thread
    std::atomic_bool  m_running;      // is the controller running?
    Status            m_status;       // cached controller status information
    std::vector<Plot> m_plots;        // buffer of user plots added with plot(...)
};