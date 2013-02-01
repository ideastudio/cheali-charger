/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "Discharger.h"
#include "Hardware.h"
#include "TimerOne.h"
#include "Utils.h"
#include "Settings.h"

Discharger::Discharger():
discharge_(0)
{
    pinMode(DISCHARGE_VALUE_PIN, OUTPUT);
    pinMode(DISCHARGE_DISABLE_PIN, OUTPUT);

    setValue(0);
    powerOff(DISCHARGING_COMPLETE);
}


AnalogInputs::ValueType Discharger::getVout()
{
    return analogInputs.getRealValue(VName);
}
AnalogInputs::ValueType Discharger::getIdischarge()
{
    return analogInputs.getRealValue(IName);
}

void Discharger::setValue(uint16_t value)
{
    if(value > DISCHARGER_UPPERBOUND_VALUE)
        value = DISCHARGER_UPPERBOUND_VALUE;
    valueSet_ = value;
    finalizeValueTintern(true);
}

uint16_t Discharger::correctValueTintern(uint16_t v)
{
    testTintern(tempcutoff_, settings.dischargeTempOff_ - Settings::TempDifference, settings.dischargeTempOff_);

    if(tempcutoff_)
        v = 0;
    return v;
}

void Discharger::finalizeValueTintern(bool force)
{
    uint16_t  v = correctValueTintern(valueSet_);
    if(v != value_ || force) {
        value_ = v;
        Timer1.pwm(DISCHARGE_VALUE_PIN, value_);
        analogInputs.resetMeasurement();
    }
}

void Discharger::setRealValue(uint16_t I)
{
    uint16_t value = analogInputs.reverseCalibrateValue(AnalogInputs::IdischargeValue, I);
    setValue(value);
}

void Discharger::powerOn()
{
    if(isPowerOn())
        return;

    hardware::setBatteryOutput(true);
    setValue(0);
    digitalWrite(DISCHARGE_DISABLE_PIN, false);
    analogInputs.powerOn();
    analogInputs.doFullMeasurement();
    state_ = DISCHARGING;
    discharge_ = 0;
}

void Discharger::powerOff(STATE reason)
{
    if(!isPowerOn() || reason == DISCHARGING)
        return;

    analogInputs.powerOff();
    setValue(0);
    digitalWrite(DISCHARGE_DISABLE_PIN, true);
    hardware::setBatteryOutput(false);
    state_ = reason;
}

void Discharger::doSlowInterrupt()
{
    if(isPowerOn()) {
        finalizeValueTintern(false);
        discharge_+=getIdischarge();
    }
}

uint16_t Discharger::getDischarge() const
{
    uint32_t retu = discharge_;
#if INTERRUPT_PERIOD_MICROSECONDS == 512
//    retu *= INTERRUPT_PERIOD_MICROSECONDS;
    retu /= (1000000/32);//*(3600/16) == TIMER_SLOW_INTERRUPT_INTERVAL

#else
#warning "INTERRUPT_PERIOD_MICROSECONDS != 512"
    retu *= INTERRUPT_PERIOD_MICROSECONDS;
    retu /= 1000000;
    retu /= 3600/TIMER_SLOW_INTERRUPT_INTERVAL;
#endif
    return retu;
}