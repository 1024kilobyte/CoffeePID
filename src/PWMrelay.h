#pragma once
#include <Arduino.h>
/*
    Based on https://github.com/AlexGyver/GyverLibs - PWMrelay
	PWMrelay - a library for generating a low-frequency PWM signal for relays (for PID controllers, etc.)
	- Customization of the PWM period
	- Supports low and high level relays
	- Non-blocking call, does not use timers (except for the system one), works on millis ()
*/

class PWMrelay {
public:
	PWMrelay(byte pin, bool dir, int period);	// pin, relay level HIGH / LOW, period
	void tick();					// tick, call as often as possible, controls the relay
	void setPWM(float duty);		// set the PWM fill factor, 0 - 100 (0-100%). With a value of 0 and 100, the tick is inactive!
	float getPWM();					// returns the PWM value
	void setPeriod(int period);		// set the PWM period in milliseconds. (default 1000ms = 1s)
	int getPeriod();				// get period
	void setLevel(bool level);		// set relay level(HIGH / LOW)
	bool isActive();				// current state of relay
	
private:
	bool _isActive = false;
	bool _dir = false;
	byte _pin = 0;
	float _duty = 0;
	int _period = 1000;
	int _activePeriod = 0;
	uint32_t _tmr = 0;
	bool _isWorking = false;
};

PWMrelay::PWMrelay(byte pin, bool dir = false, int period = 1000) {
	_pin = pin;
	_dir = !dir;
	pinMode(_pin, OUTPUT);
	digitalWrite(_pin, _dir);  // immediately off
	PWMrelay::setPeriod(period);
}

void PWMrelay::tick() {
	if (_isWorking) {
		if (millis() - _tmr >= (_isActive ? _activePeriod : (_period - _activePeriod))) {
			_tmr = millis();
			_isActive = !_isActive;
			digitalWrite(_pin, _isActive ^ _dir);
		}
	}
}

void PWMrelay::setPWM(float duty) {
	if(duty == _duty) return;
	_duty = duty;
	_activePeriod = (_duty * _period / 100.0) + 0.5;
	_isWorking = false;
	if (_activePeriod <= 0) {
		digitalWrite(_pin, _dir);		// off
		_isActive = false;				// for isActive()
		_activePeriod = 0;
	}
	else if (_activePeriod >= _period) {
		digitalWrite(_pin, !_dir);	// on
		_isActive = true;			// for isActive()
		_activePeriod = _period;
	}
	else _isWorking = true;
}

float PWMrelay::getPWM() {
	return _duty;
}

void PWMrelay::setPeriod(int period) {
	_period = period;
	PWMrelay::setPWM(_duty);	// in case of "hot" period change
}

int PWMrelay::getPeriod() {
	return _period;
}

void PWMrelay::setLevel(bool level) {
	_dir = !level;
}

bool PWMrelay::isActive() {
	return _isActive;
}
