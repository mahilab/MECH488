// include necessary files from MEL library
#include <MEL/Core.hpp>
#include <MEL/Mechatronics.hpp>
#include <MEL/Math.hpp>
#include <MEL/Daq/NI/MyRio/MyRio.hpp>
#include <MEL/Communications/MelNet.hpp>
#include <MEL/Logging/Csv.hpp>

// use mel namespace so we can say foo() instead of mel::foo()
using namespace mel;

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

//=============================================================================
// MAIN
//=============================================================================

// main(), the entry point for all C/C++ programs (all code starts here!)
int main() {
    // register our handler so its called when ctrl+c is pressed
    register_ctrl_handler(my_handler);
    // create a myRIO object
    MyRio myrio;
    // open communication with myRIO FPGA
    myrio.open();
    // enable myRIO to set intial values
    myrio.enable();
    // enable encoder 0 (A = DIO 0, B = DIO 2)
    myrio.mspC.encoder.enable_channel(0);
    // set units per count on encoder (get_position() will return radians, make 360.0/500.0 if degrees prefered)
    myrio.mspC.encoder[0].set_units_per_count(2 * PI / 500.0);
    // zeros the encoder, meaning that its CURRENT position is 0 radians
    myrio.mspC.encoder[0].zero();
    // configure MSPC DIO[1] to be output (all DIOs are inputs by default)
    myrio.mspC.DIO[1].set_direction(Out);
    // create MelNet so we can stream data to host PC
    MelNet mn(55002, 55001, "172.22.11.1");
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
    // path to csv file (absolute_folder will be created in system root)
    std::string filepath = "/home/admin/MECH488/groupXX.csv";
    // vector of double vectors which will hold information to write
    std::vector<std::vector<double>> data;
    // start and run our control loop until STOP is true
    while(!STOP) {

        // get real world input values
        myrio.update_input();

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
            myrio.set_led(0, true);
        else
            myrio.set_led(0, false);

        // digital, button, led loop (connect DIO 1 to DIO 3)
        if (myrio.is_button_pressed())
            myrio.mspC.DIO[1] = High;
        else
            myrio.mspC.DIO[1] = Low; 

        if (myrio.mspC.DIO[3] == High)
            myrio.set_led(3, true);
        else
            myrio.set_led(3, false);

        // encoder read counts (connect A to DIO 0, B to DIO 2)
        int raw_counts = myrio.mspC.encoder[0];
        // encoder read position (performs conversion using factor passed to set_units_per_count())
        double radians = myrio.mspC.encoder[0].get_position();

        // --------------------------------------------------------------------
        // !!! END YOUR CONTROL IMPLEMENTATION !!!
        // --------------------------------------------------------------------

        //append data to a vector to use for datalogging
        data.push_back({t.as_seconds(), volts_write, volts_read});
        // stream doubles of interest (add/remove variables if desired)
        mn.send_data({volts_write, volts_read, (double)raw_counts, radians});
        // set real world output values
        myrio.update_output();
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