#include <Mahi/Gui.hpp>
#include <Mahi/Com.hpp>

using namespace mahi::gui;
using namespace mahi::util;
using namespace mahi::com;

class PendulumGui : public Application {
public:
    PendulumGui() : Application(500,500,"Pendulum GUI") {
         
    }
};

int main(int argc, char const *argv[])
{
    PendulumGui gui;
    gui.run();
    return 0;
}
