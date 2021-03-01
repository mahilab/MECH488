#include "IPendulum.hpp" // for IPendulum 
#include "Iir.h"         // for filters

//=============================================================================
// PENDULUM
//=============================================================================

/// Your pendulum implementation inherits from IPendulum.
class MyPendulum : public IPendulum {
public:
    /// Constructor. Called when we make our MyPendulum instance in main().
    MyPendulum() {
        // initialize our filter(s)
        double fSample = 1000; // sampling frequency [Hz]
        double fCutoff = 10;   // cutoff frequency   [Hz]
        butt.setup(fSample,fCutoff);

        // do any other initialization you need here too ...
    }

    /// Part 1: Implement control with encoder position feedback.
    double control_encoder(double t, int counts) override {

        // This function should compute an amplifier command voltage given the
        // current controller time in seconds and encoder position in counts. 
        // Your responsibilities include but are not limited to:   

        // 1. Convert encoder counts to angular position.
        // 2. Differentiate and/or integrate angular position for your PID implementation.
        // 3. Filter these state variables as needed (use butt.filter() or your own method).
        // 4. Implement a PID, PI, or PD controller to compute motor torque.
        // 5. Convert motor torque to command voltage (torque -> amps -> volts)
        // 6. Return the command voltage in [V].

        // TIP: Use the plot() function to stream any data you need to visualize to the GUI plotter:
        plot("MyPlot1", sin(2*PI*0.5*t)); 
        // plot("MyPlot2", sin(2*PI*1.0*t)); 

        double command_voltage = sin(2*PI*0.5*t);
        return command_voltage;
    }

    /// Part 2: Implement control with Midori potentiometer position feedback.
    double control_midori(double t, double midori_volts) override {

        // This function should compute an amplifier command voltage given the
        // current controller time in seconds and Midori position in volts. 
        // Your responsibilities include but are not limited to:   

        // 1. Convert Midori potentiometer voltage to angular position.
        // 2. Differentiate and/or integrate angular position for your PID implementation.
        // 3. Filter these state variables as needed (use butt.filter() or your own method).
        // 4. Implement a PID, PI, or PD controller to compute motor torque.
        // 5. Convert motor torque to command voltage (torque -> amps -> volts)
        // 6. Return the command voltage in [V].

        // TIP: Use the plot() function to stream any data you need to visualize to the GUI plotter:
        plot("MyPlot3", sin(2*PI*0.5*t)); 
        // plot("MyPlot4", sin(2*PI*1.0*t)); 

        double command_voltage = sin(2*PI*0.5*t);
        return command_voltage;
    }
public:
    Iir::Butterworth::LowPass<2> butt; // A 2nd order low-pass Butterworth filter. Usage:
                                       // double output = butt.filter(input);
                                       // Make as many of these as you need.There are also 
                                       // other filter types in Iir:: namespace.
};

//=============================================================================
// MAIN
//=============================================================================

// main is the entry point for the program; everything starts here
int main(int argc, char const *argv[]) {    
    // create an instance of your pendulum
    MyPendulum pend;
    // run the pendulum at 1 kHz (don't increase past 2 kHz)
    pend.run(1000_Hz);
    // return 0 for success
    return 0;
}
