#include <iostream>
#include <Mahi/Daq.hpp>   // for MyRio
#include <Mahi/Robo.hpp>  // for Butterworth
#include <Mahi/Util.hpp>  // for CtrlEvent, Timer, Time

// use namespace so we can say foo() instead of mahi::daq::foo() etc.
using namespace mahi::daq;
using namespace mahi::robo;
using namespace mahi::util;

//=============================================================================
// GLOBALS AND FREE FUNCTIONS
//=============================================================================

// global stop flag
ctrl_bool STOP(false);

// control handler function, called when Ctrl+C is pressed in terminal
bool my_handler(CtrlEvent event) {
    if (event == CtrlEvent::CtrlC || event == CtrlEvent::Close) {
        STOP = true;
        print("Application Terminated");
    }
    return true;
}

int main(int argc, char const *argv[])
{
    // enable verbose logging
    MahiLogger->set_max_severity(Severity::Verbose);
    // register our handler so its called when ctrl+c is pressed
    register_ctrl_handler(my_handler);
    // create a myRIO object
    MyRio myrio;
    // enable myRIO to set initial values
    myrio.enable();
    // enable encoder 0
    myrio.mspC.encoder.set_channels({0});
    // set units per count on encoder (get_position() will return radians, make 360.0/500.0 if degrees prefered)
    myrio.mspC.encoder.units[0] = 2 * PI / 500.0;
    // zeros the encoder, meaning that its CURRENT position is 0 radians
    myrio.mspC.encoder.zero(0);
    // enable MSPC DO[1] (all DIOs are inputs by default)
    myrio.mspC.DO.set_channels({1});
    // create your constants that you will use for control here
    double k_theta = 0.0;
    double k_omega = 0.0;
    // create Timer for our control loop
    Timer timer(hertz(1000));
    // create a Time point t
    Time t = Time::Zero;
    // create a second order butterworth filter to filter velocity
    Butterworth butt(2,hertz(10),hertz(1000));
    // headers for the csv file for datalogging
    std::vector<std::string> header = {"Time (s)", "Write (V)", "Read (V)"};
    // path to csv file (relative to current executable locaton)
    std::string filepath = "group_XX.csv";
    // vector of double vectors which will hold information to write
    std::vector<std::vector<double>> data;
    // start and run our control loop until STOP is true
    while(!STOP) {

        // get real world input values
        myrio.read_all();

        // --------------------------------------------------------------------
        // !!! BEGIN YOUR CONTROL IMPLEMENTATION !!!
        // --------------------------------------------------------------------
        //
        // The code here is not what you will use for you project,
        // but can serve as an example for implementation
        //
        // analog loopback (connect AO 0 to AI 0)
        double volts_write = std::sin(2 * PI * t.as_seconds());
        myrio.mspC.AO[0] = volts_write;
        double volts_read  = myrio.mspC.AI[0];

        if (volts_read > 0)
            myrio.LED[0] = true;
        else
            myrio.LED[0] = false;

        // digital, button, led loop (connect DIO 1 to DIO 3)
        if (myrio.is_button_pressed())
            myrio.mspC.DO[1] = TTL_HIGH;
        else
            myrio.mspC.DO[1] = TTL_LOW; 

        if (myrio.mspC.DI[3] == TTL_HIGH)
            myrio.LED[3] = true;
        else
            myrio.LED[3] = false;

        // encoder read counts (connect A to DIO 0, B to DIO 2)
        int raw_counts = myrio.mspC.encoder[0];
        // encoder read position (performs conversion using factor passed to set_units_per_count())
        double radians = myrio.mspC.encoder.positions[0];

        // --------------------------------------------------------------------
        // !!! END YOUR CONTROL IMPLEMENTATION !!!
        // --------------------------------------------------------------------

        //append data to a vector to use for datalogging
        data.push_back({t.as_seconds(), volts_write, volts_read});
        // set real world output values
        myrio.write_all();
        // wait timer and then get elapsed time
        t = timer.wait();
    }
    // write the header
    csv_write_row(filepath, header);    
    // append the data
    csv_append_rows(filepath, data);
    // return 0 to indicate success
    return 0;
}
