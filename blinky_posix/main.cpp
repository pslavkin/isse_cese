#include "qpcpp.hpp"
#include <iostream>
#include <cstdlib> // for exit()
#include "blinky.hpp"

using namespace std;
using namespace QP;


extern "C" void Q_onError(char const * const module, int loc) 
{
    cout << "Assertion failed in " << module << ':' << loc << endl;
    exit(-1);
}

void QF::onStartup(void) {}
void QF::onCleanup(void) {}
void QF::onClockTick(void) 
{
    QTimeEvt::TICK_X(0U, nullptr);  // QTimeEvt clock tick processing
}



QActive * const AO_Blinky = &Blinky::inst;
class Test     AO_Test(1);


int main() {
    QF::init(); // initialize the framework

    static QEvtPtr blinky_queueSto[10];
    static QEvtPtr test_queueSto[10];
    AO_Blinky->start(1U, // priority
                     blinky_queueSto, Q_DIM(blinky_queueSto),
                     nullptr, 0U); // no stack

    AO_Test.start(2U, // priority
                     test_queueSto, Q_DIM(test_queueSto),
                     nullptr, 0U); // no stack
    return QF::run(); // run the QF application
}

