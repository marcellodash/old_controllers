#ifndef NESCONTROLLER_H
#define NESCONTROLLER_H

#include "BaseController.h"

class NESController : public BaseController {
public:
    char NES_raw_dump[8]; // Temp dump location

    struct JoystickStatusStruct *JoyStatus;

    NESController() {};
    NESController(struct JoystickStatusStruct *JoyStatus);

    void init(char pins_avail);
    void print_status(short int cnum);
    void detect_controllers(char pins_avail);
    void get();
    void translate_raw_data();
    void clear_dump();

    short int pinmask;

    void read_state();
    void fillStatus(struct JoystickStatusStruct *joylist);
};

#endif /* NESCONTROLLER_H */