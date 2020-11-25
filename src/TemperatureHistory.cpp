#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include "TemperatureHistory.h"

extern WebSocketsServer webSocket;
extern float global_current_temp, global_current_temp_f;
extern MeanBuffer<float> heaterPowerLog;

void TemperatureHistoryBuffer::writeTemperature() {
	unsigned long time = millis();
	static float lastTemperature = 0;
	if (time >= _next_update || fabs(global_current_temp_f - lastTemperature) >= 0.1) {
		push_back(TemperaturePoint{ time,
									global_current_temp_f,
									static_cast<uint8_t>(heaterPowerLog.back() * UCHAR_MAX),
									static_cast<uint8_t>(heaterPowerLog.getMean() * UCHAR_MAX)
			});
		lastTemperature = global_current_temp_f;
		if (time >= _next_update) _next_update += _update_interval;
	}
}

void TemperatureHistoryBuffer::loop() {
	StaticJsonDocument<512> doc;
	String jsonPayload;

	for (int i = 0; i < _task_list_count; i++) { //send history
		SendTask& task = _send_task_list[i];
		uint8_t client_num = task.client_num;
		if (task.last_point == 0) task.last_point = size() - 1;

		auto getPos = [=](size_t index) {
			index += getHead();
			if (index >= capacity()) index -= capacity();
			return index;
		};
		auto startPos = getPos(task.start_point);
		auto lastPos = getPos(task.last_point);

		doc.clear();
		JsonObject sendBinary = doc.createNestedObject("sendBinary");

		auto sendBuf = [&](size_t start_pos, size_t end_pos) mutable {
			size_t length = end_pos - start_pos + 1;
			sendBinary["length"] = length;
			jsonPayload = "";
			serializeJson(doc, jsonPayload);
			webSocket.sendTXT(client_num, jsonPayload);
			webSocket.sendBIN(client_num, getBuffPtr() + start_pos * sizeof(TemperaturePointShort), length * sizeof(TemperaturePointShort), false);
		};

		TemperaturePoint tp;
		if (task.start_point == 0) {
			tp.time = _frontTime;
			tp.temperature = static_cast<float>(_frontTemperature) / 100.0;
		}
		else tp = (*this)[task.start_point - 1];

		sendBinary["frontTime"] = tp.time;
		sendBinary["frontTemperature"] = tp.temperature * 100.0;
		if (lastPos >= startPos) {
			sendBinary["lastPart"] = true;
			sendBuf(startPos, lastPos);
		}
		else {
			sendBuf(startPos, capacity() - 1);
			doc.clear();
			sendBinary = doc.createNestedObject("sendBinary");
			sendBinary["lastPart"] = true;
			sendBuf(0, lastPos);
		}

		//delete task
		_task_list_count--;
		for (int n = i; n < _task_list_count; n++) _send_task_list[n] = _send_task_list[n + 1]; //shift tasks
		i--;
	}

	writeTemperature();
}

void TemperatureHistoryBuffer::mkSendTask(uint8_t client_num, unsigned long from, unsigned long to) {
	if (empty()) return;

	if (_task_list_count + 1 > _send_task_max_count) {
		_send_task_max_count++;
		if (_send_task_max_count == 1) _send_task_list = (SendTask*)malloc(sizeof(SendTask));
		else _send_task_list = (SendTask*)realloc((void*)_send_task_list, _send_task_max_count * sizeof(SendTask));
	}

	SendTask& task = _send_task_list[_task_list_count];
	task.client_num = client_num;
	task.start_point = 0;
	task.last_point = 0;

	auto it = circular_buffer::begin();
	auto time = _frontTime;
	if (from != 0) {
		for (; it <= circular_buffer::end(); it++) {
			time += it->timeDifference;
			if (time >= from) break;
		}
		if (it == circular_buffer::end()) return; //Nothing to send;
		task.start_point = it - circular_buffer::begin();
	}

	for (int i = 0; i < _task_list_count; i++) {
		if (task == _send_task_list[i]) {
			return; //already exists
		}
	}

	_task_list_count++;

	if (to != 0) {
		for (; it <= circular_buffer::end(); it++) {
			time += it->timeDifference;
			if (time >= to) break;
		}
		if (it == circular_buffer::end()) return; //Send all
		task.last_point = it - circular_buffer::begin();
	}

	for (int i = 0; i < _task_list_count - 1; i++) {
		if (task == _send_task_list[i]) {
			_task_list_count--; //already exists, delete task
			return;
		}
	}
}

