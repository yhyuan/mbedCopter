/*
 * altitude.cpp
 *
 * Copyright (c) 2013, Thomas Buck <xythobuz@xythobuz.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "altitude.h"
#include "errors.h"

#include <math.h>

// inspired by https://www.sparkfun.com/tutorials/253 :)

Altitude::Altitude(I2C *i) {
    i2c = i;
}

error_t Altitude::init() {
    return readCalibration();
}

error_t Altitude::read(uint8_t add, int16_t *data) {
    char buf[2];
    buf[0] = add;
    if (i2c->write(address, buf, 1, true))
        return ERR_ALT_WRITE;
    if (i2c->read(address, buf, 2, false))
        return ERR_ALT_READ;
    *data = buf[0] << 8;
    *data |= buf[1];
    return SUCCESS;
}

error_t Altitude::readCalibration() {
    error_t error = 0;
    if (
    (error = read(0xAA, &ac1)) ||
    (error = read(0xAC, &ac2)) ||
    (error = read(0xAE, &ac3)) ||
    (error = read(0xB0, (int16_t *)&ac4)) ||
    (error = read(0xB2, (int16_t *)&ac5)) ||
    (error = read(0xB4, (int16_t *)&ac6)) ||
    (error = read(0xB6, &b1)) ||
    (error = read(0xB8, &b2)) ||
    (error = read(0xBA, &mb)) ||
    (error = read(0xBC, &mc)) ||
    (error = read(0xBE, &md))
        ) {
        return error;
    } else {
        return SUCCESS;
    }
}

error_t Altitude::readUT(uint16_t *data) {
    char buf[2];
    buf[0] = 0xF4;
    buf[1] = 0x2E;
    if (i2c->write(address, buf, 2, false))
        return ERR_ALT_WRITE;

    wait(0.0045);

    if (error_t error = read(0xF6, (int16_t *)data))
        return error;

    return SUCCESS;
}

error_t Altitude::readUP(uint32_t *data) {
    char buf[3];
    buf[0] = 0xF4;
    buf[1] = 0x34 + (OSS << 6);
    if (i2c->write(address, buf, 2, false))
        return ERR_ALT_WRITE;

    wait(0.001f * (float)(2 + (3 << OSS)));

    buf[0] = 0xF6;
    if (i2c->write(address, buf, 1, false))
        return ERR_ALT_WRITE;

    if (i2c->read(address, buf, 3, false))
        return ERR_ALT_READ;

    uint32_t up = (((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[2]) >> (8 - OSS);
    *data = up;
    return SUCCESS;
}

error_t Altitude::readTemperature(uint16_t *data) {
    int32_t x1, x2;
    uint16_t ut, t;

    if (error_t error = readUT(&ut))
        return error;

    x1 = (((int32_t)ut - (int32_t)ac6) * (int32_t)ac5) >> 15;
    x2 = ((int32_t)mc << 11) / (x1 + md);
    b5 = x1 + x2;
    t = (b5 + 8) >> 4;
    *data = t;
    return SUCCESS;
}

error_t Altitude::readPressure(int32_t *data) {
    int32_t x1, x2, x3, b3, b6, p;
    uint32_t b4, b7, up;

    if (error_t error = readUP(&up))
        return error;

    b6 = b5 - 4000;
    x1 = (b2 * (b6 * b6) >> 12) >> 11;
    x2 = (ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t)ac1) * 4 + x3) << OSS) + 2) >> 2;

    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (ac4 * (uint32_t)(x3 + 32768)) >> 15;

    b7 = ((uint32_t)(up - b3) * (50000 >> OSS));
    if (b7 < 0x80000000)
        p = (b7 << 1) / b4;
    else
        p = (b7 / b4) << 1;

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p += (x1 + x2 + 3791) >> 4;

    *data = p;
    return SUCCESS;
}

error_t Altitude::read(uint16_t *temperature, int32_t *pressure) {
    if (error_t error = readTemperature(temperature))
        return error;

    return readPressure(pressure);
}

float Altitude::calculateAltitude(int32_t pressure) {
    float alt = (float)pressure / 100.0 / 1013.25; // pressure at sea level
    alt = pow(alt, (1.0 / 5.255));
    alt = 1 - alt;
    return 44330 * alt;
}
