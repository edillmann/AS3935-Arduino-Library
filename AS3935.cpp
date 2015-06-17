/*
   AS3935.cpp - AS3935 Franklin Lightning Sensor™ IC by AMS library
   Copyright (c) 2012 Raivis Rengelis (raivis [at] rrkb.lv). All rights reserved.
   Copyright (c) 2015 Eric Dillmann. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "AS3935.h"
// I2c library by Wayne Truchsess
#include <I2C.h>

volatile int16_t pulse;

AS3935::AS3935(uint8_t irq,uint8_t addr)
{
	//configure the uint16_terrupt
	_IRQ = irq;
	//configure the address
	_ADDR = addr;
}

uint8_t AS3935::getTune()
{
	return _tune;
}

uint8_t AS3935::_ffsz(uint8_t mask)
{
	uint8_t i = 0;
	if (mask)
		for (i = 1; ~mask & 1; i++)
			mask >>= 1;
	return i;
}

void AS3935::setTune(int tuneValue)
{
	_tune = tuneValue;
	registerWrite(AS3935_TUN_CAP,tuneValue);
	delay(2);
	powerUp();
}

void AS3935::registerWrite(uint8_t reg, uint8_t mask, uint8_t data)
{
	//read 1 uint8_t
	I2c.read((uint8_t)_ADDR, (uint8_t)reg, (uint8_t)0x01);
	//put it to regval
	uint8_t regval = I2c.receive();
	//do masking
	regval &= ~(mask);
	if (mask)
		regval |= (data << (_ffsz(mask)-1));
	else
		regval |= data;
	//write the register back
	I2c.write(_ADDR, reg, regval);
}

uint8_t AS3935::registerRead(uint8_t reg, uint8_t mask)
{
	//read 1 uint8_t
	I2c.read((uint8_t)_ADDR, (uint8_t)reg, (uint8_t)0x01);
	//put it to regval
	uint8_t regval = I2c.receive();
	//mask
	regval = regval & mask;
	if (mask)
		regval >>= (_ffsz(mask)-1);
	return regval;
}

void AS3935::reset()
{
	//write to 0x3c, value 0x96
	I2c.write((uint8_t)_ADDR, (uint8_t)0x3c, (uint8_t)0x96);
	delay(2);
}

static void pulseCounter() {
	pulse++;
}

bool AS3935::calibrate()
{
	int16_t target = 3125, currentcount = 0, bestdiff = INT_MAX, currdiff = 0;
	uint8_t bestTune = 0, currTune = 0;
	unsigned long setUpTime;
	uint16_t currIrq, prevIrq;
	// set lco_fdiv divider to 0, which translates to 16
	// so we are looking for 31250Hz on irq pin
	// and since we are counting for 100ms that translates to number 3125
	// each capacitor changes second least significant digit
	// using this timing so this is probably the best way to go
	registerWrite(AS3935_LCO_FDIV,0);
	registerWrite(AS3935_DISP_LCO,1);
	// tuning is not linear, can't do any shortcuts here
	// going over all built-in cap values and finding the best
	attachInterrupt(0, pulseCounter, RISING);
	
	for (currTune = 0; currTune <= 0x0F; currTune++) {
		registerWrite(AS3935_TUN_CAP,currTune);
		// let it settle
		delay(2);
		noInterrupts();
		pulse = 0;
		interrupts();
		delay(100);
		noInterrupts();
		currentcount = pulse;
		interrupts();
		
		currdiff = abs(target - currentcount);
		if(bestdiff > currdiff) {
			bestdiff = currdiff;
			bestTune = currTune;
		}
	}
	detachInterrupt(0);
	registerWrite(AS3935_TUN_CAP,bestTune);
	delay(2);
	registerWrite(AS3935_DISP_LCO,0);
	// and now do RCO calibration
	I2c.write((uint8_t)_ADDR, (uint8_t)0x3D, (uint8_t)0x96);
	delay(3);
	// if error is over 109, we are outside allowed tuning range of +/-3.5%
	_tune = bestTune;
	return bestdiff > 109 ? false : true;
}

void AS3935::powerDown()
{
	registerWrite(AS3935_PWD,1);
}

void AS3935::powerUp()
{
	registerWrite(AS3935_PWD,0);
	I2c.write((uint8_t)_ADDR, (uint8_t)0x3D, (uint8_t)0x96);
	delay(3);
}

uint16_t AS3935::interruptSource()
{
	return registerRead(AS3935_INT);
}

void AS3935::disableDisturbers()
{
	registerWrite(AS3935_MASK_DIST,1);
}

void AS3935::enableDisturbers()
{
	registerWrite(AS3935_MASK_DIST,0);
}

uint16_t AS3935::getMinimumLightnings()
{
	return registerRead(AS3935_MIN_NUM_LIGH);
}

uint16_t AS3935::setMinimumLightnings(uint16_t minlightning)
{
	registerWrite(AS3935_MIN_NUM_LIGH,minlightning);
	return getMinimumLightnings();
}

uint16_t AS3935::lightningDistanceKm()
{
	return registerRead(AS3935_DISTANCE);
}

void AS3935::setIndoors()
{
	registerWrite(AS3935_AFE_GB,AS3935_AFE_INDOOR);
}

void AS3935::setOutdoors()
{
	registerWrite(AS3935_AFE_GB,AS3935_AFE_OUTDOOR);
}

uint16_t AS3935::getNoiseFloor()
{
	return registerRead(AS3935_NF_LEV);
}

uint16_t AS3935::setNoiseFloor(uint16_t noisefloor)
{
	registerWrite(AS3935_NF_LEV,noisefloor);
	return getNoiseFloor();
}

uint16_t AS3935::getSpikeRejection()
{
	return registerRead(AS3935_SREJ);
}

uint16_t AS3935::setSpikeRejection(uint16_t srej)
{
	registerWrite(AS3935_SREJ, srej);
	return getSpikeRejection();
}

uint16_t AS3935::getWatchdogThreshold()
{
	return registerRead(AS3935_WDTH);
}

uint16_t AS3935::setWatchdogThreshold(uint16_t wdth)
{
	registerWrite(AS3935_WDTH,wdth);
	return getWatchdogThreshold();
}

void AS3935::clearStats()
{
	registerWrite(AS3935_CL_STAT,1);
	registerWrite(AS3935_CL_STAT,0);
	registerWrite(AS3935_CL_STAT,1);
}
