// include necessary files from MEL library
#include <MEL/Core.hpp>
#include <MEL/Mechatronics.hpp>
#include <MEL/Math.hpp>
#include <MEL/Daq/NI/MyRio/MyRio.hpp>
#include <MEL/Communications/MelNet.hpp>

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
    // enable myRIO
    myrio.enable();
    // configure MSPC DIO[0] to be output (all DIOs are inputs by default)
    myrio.mspC.DIO[0].set_direction(Out);
    // create MelNet so we can stream data to host PC
    MelNet mn(55002, 55001, "172.22.11.1");
    // create Timer for our control loop
    Timer timer(hertz(1000));
    // create a Time point t
    Time t = Time::Zero;
    // start our control loop
    while(!STOP) {
        // get current real world input values
        myrio.update_input();

        // !!! BEGIN YOUR CONTROL IMPLEMENTATION !!!
        // -----------------------------------------
        //
        // The code here is not what you will use for you project,
        // but can serve as an example for implementation
        //
        // analog loopback example (connect AO0 to AI0)
        double volts_write = std::sin(2 * PI * t.as_seconds());
        myrio.mspC.AO[0] = volts_write;
        double volts_read  = myrio.mspC.AI[0];
        if (volts_read > 0)
            myrio.set_led(0, true);
        else
            myrio.set_led(0, false);
        // digital, button, led loop back (connect DIO0 to DIO1)
        if (myrio.is_button_pressed())
            myrio.mspC.DIO[0] = High;
        else
            myrio.mspC.DIO[0] = Low; 
        if (myrio.mspC.DIO[1] == High)
            myrio.set_led(3, true);
        else
            myrio.set_led(3, false);
        // ---------------------------------------
        // !!! END YOUR CONTROL IMPLEMENTATION !!!

        // stream doubles of interest (add/remove variables if desired)
        mn.send_data({volts_write, volts_read});
        // set current real world output values
        myrio.update_output();
        // wait timer and then get current time
        t = timer.wait();
    }
    // return success
    return 0;
}