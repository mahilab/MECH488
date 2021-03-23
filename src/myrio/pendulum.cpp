#include "IPendulum.hpp" // for IPendulum 
#include "Iir.h"         // for filters

//=============================================================================
// PENDULUM
//=============================================================================

using namespace mahi::util;

/// Your pendulum implementation inherits from IPendulum.
class MyPendulum : public IPendulum {
public:
    /// Constructor. Called when we make our MyPendulum instance in main().
    MyPendulum(double sample_freq_): sample_freq(sample_freq_) {      
        
        ///// IF YOU NEED TO DO ANY SETUP FOR VARIABLES, THAT GOES HERE /////
        // my_filter.setup(sample_freq, 10.0);
        // ... any other setup you want to do 
        
        ///// END SETUP /////
    }

    /// Part 1: Implement control with Midori potentiometer position feedback.
    double control_midori(double t, double midori_volts) override {
        // This function should compute an amplifier command voltage given the
        // current controller time in seconds and Midori position in volts. 

        static double volts_last = 0.0;

        // See tips in the wiki
        double my_var = sin(2*PI*1.0*t);
        plot("My Variable 2", my_var); 

        // See tips in the wiki
        volts_last = midori_volts;

        double command_voltage = 0;
        return command_voltage;
    }

    /// Part 2: Implement control with encoder position feedback.
    double control_encoder(double t, int counts) override {

        // This function should compute an amplifier command voltage given the
        // current controller time in seconds and encoder position in counts. 

        // See tips in the wiki for static objects
        static double counts_last = 0.0;
        static Butterworth my_filter(2, hertz(sample_freq), hertz(10));
        
        // how to use the filter
        double counts_filtered = my_filter.update(counts);

        // See tips in the wiki for plotting
        double my_var = sin(2*PI*0.5*t);
        plot("My Variable 1", my_var); 

        // See tips in the wiki
        counts_last = counts;

        double command_voltage = 0;
        return command_voltage;
    }

public:
    double sample_freq;       // our samplerate in Hz
    
    ///// VARIABLES THAT YOU WANT TO KEEP VALUE BETWEEN SAMPLES GO HERE! /////
    // LowPassFilter my_filter;  // lowpass filter to filter encoder
    // double counts_last = 0.0; // variable to store previous value of counts
    // double volts_last = 0.0;  // variable to store previous value of volts
    // ... whatever other variables you want to add
    ///// END VARIABLES ///// 
};

//=============================================================================
// MAIN
//=============================================================================

// main is the entry point for the program; everything starts here
int main(int argc, char const *argv[]) {    

    ////// ADJUST YOUR SAMPLE RATE HERE //////
    Frequency sample_rate = hertz(1000);
    //////  DON'T TOUCH ANYTHING ELSE  ///////

    // create an instance of your pendulum
    MyPendulum pend(sample_rate.as_hertz());
    // run the pendulum at 1 kHz (don't increase past 2 kHz)
    pend.run(sample_rate);
    // return 0 for success
    return 0;
}
