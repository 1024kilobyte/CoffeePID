#include <limits.h>
#include "circular_buffer.h"

struct TemperaturePoint {  //sizeof(TemperaturePoint) = 12  (10 with packed attribute)
	unsigned long time;
	float temperature;
	uint8_t power;
	uint8_t meanPower;
	bool operator<(const unsigned long val) const { return time < val; }
};

struct __attribute__((packed)) TemperaturePointShort { //sizeof(TemperaturePointShort) = 5, this is struct to reduce memory consuption by history buffer
	uint16_t timeDifference;					       //time difference from previous point
	int8_t temperatureDifference;					   //temperature difference from previous point (* 100)
	uint8_t power;									   //power, 0 - 0%, 255 -  100%			
	uint8_t meanPower;								   //moving average of power (it's possible to remove this field and calculate it on the client side)	
};

struct SendTask {
	uint8_t client_num;
	size_t start_point;
	size_t last_point;
	bool operator==(const SendTask& other) const { 
		return client_num == other.client_num && start_point == other.start_point && last_point == other.last_point; }
	bool operator!=(const SendTask& other) const { return !(*this == other); }
};


class TemperatureHistoryBuffer : public circular_buffer<TemperaturePointShort> {
public:
	explicit TemperatureHistoryBuffer(size_type size) : circular_buffer(size)
	{
		_send_task_list = NULL;
		_send_task_max_count = 0;
		_next_update = ULONG_MAX;
		_task_list_count = 0;
		_frontTime = _backTime = 0;
		_frontTemperature = _backTemperature = 0;
	}

	~TemperatureHistoryBuffer() { free(_send_task_list); }

	TemperaturePoint front() {
		TemperaturePointShort tp = circular_buffer::front();
		return TemperaturePoint{ _frontTime + tp.timeDifference,
								 static_cast<float>(_frontTemperature + tp.temperatureDifference)/100.0,
								 tp.power, 
								 tp.meanPower };
	}

	TemperaturePoint back() {
		return TemperaturePoint{ _backTime, 
								 static_cast<float>(_backTemperature)/100.0, 
								 circular_buffer::back().power, 
								 circular_buffer::back().meanPower };
	}
	
	TemperaturePoint operator[](size_type index) {    //make TemperaturePoint from TemperaturePointShort
		if (index >= size()) return TemperaturePoint();
		unsigned long time = _frontTime;
		int temperature = _frontTemperature;
		for (iterator it = circular_buffer::begin(); it <= circular_buffer::begin() + index; it++) {
			time += it->timeDifference;
			temperature += it->temperatureDifference;
		}
		iterator it = circular_buffer::begin() + index;
		return TemperaturePoint{ time, static_cast<float>(temperature) / 100.0, it->power, it->meanPower };
	}

	void push_back(const TemperaturePoint& item) {
		TemperaturePointShort tp;

		if (size() == 0) {
			_frontTime = item.time;
			_backTime = item.time;
			_frontTemperature = static_cast<int>(item.temperature * 100.0);
			_backTemperature = _frontTemperature;
			tp.timeDifference = 0;
			tp.temperatureDifference = 0;
		}
		else {
			uint16_t timeDiff = static_cast<uint16_t>(item.time - _backTime);
			tp.timeDifference = timeDiff;
			_backTime += timeDiff;
			int8_t tempDiff = static_cast<int8_t>(item.temperature * 100.0 - _backTemperature);
			tp.temperatureDifference = tempDiff;
			_backTemperature += tempDiff;
		}
		tp.power = item.power;
		tp.meanPower = item.meanPower;
		if (circular_buffer::full()) pop_front();
		circular_buffer::push_back(tp);
	}

	value_type pop_front() {
		if (empty()) { return value_type(); }
		value_type oldFront = circular_buffer::pop_front();
		_frontTime += oldFront.timeDifference;
		_frontTemperature += oldFront.temperatureDifference;
		return oldFront;
	}

	void setNextTime() {  //setting start time for writeTemperature()
		_next_update = millis();
		_next_update = _next_update - _next_update % 1000;  //truncate to sec
	}

	void writeTemperature();
	void loop();
	void mkSendTask(uint8_t client_num, unsigned long from, unsigned long to);

protected:
	const unsigned long _update_interval = 1000;
	unsigned long _next_update;
private:
	unsigned long _frontTime, _backTime;
	int _frontTemperature, _backTemperature;
	SendTask* _send_task_list;
	int _send_task_max_count;
	int _task_list_count;
};
