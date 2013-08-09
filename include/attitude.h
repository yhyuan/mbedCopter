#include "kalman.h"
#include "sensors.h"

#ifndef _ATTITUDE_H
#define _ATTITUDE_H

class Attitude {
public:
    Attitude(Gyro *g, Acc *a);
    int calculate(); // 0 on success

    double roll, pitch, yaw;

private:
    Kalman pitchFilter, rollFilter;
    Gyro *gyro;
    Acc *acc;
};

#endif