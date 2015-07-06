#ifndef SNESCONTROLLER_H
#define SNESCONTROLLER_H

#include "BaseController.h"

class SNESController : public BaseController {
public:
    uint8_t raw_dump[16]; // Temp dump location

    SNESController(JoystickStatus *JoyStatus, uint8_t* global_pins, const char* controller_name)
        : BaseController(JoyStatus, global_pins, controller_name) {};

    void init();
    void setup_pins();
    void print_status(short int cnum);
    void detect_controllers(uint8_t pins_avail);
    void get();
    void translate_raw_data();
    void clear_dump();

    void pulse_latch();
    void pulse_clock();

    void read_state();
    void fillJoystick(JoystickStatus *joystick, uint8_t datamask);
};

#endif /* SNESCONTROLLER_H */
