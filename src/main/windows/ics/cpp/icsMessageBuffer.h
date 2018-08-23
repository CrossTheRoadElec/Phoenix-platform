//#pragma once
//#include "icsnVC40.h"
//
//class ICSMessageBuffer {
//private:
//	icsSpyMessage * _msgs;
//	int _capacity;
//	int _in;
//	int _ou;
//	int _cnt;
//	std::recursive_timed_mutex _mutex;
//
//public:
//	ICSMessageBuffer()
//	{
//		_msgs = new icsSpyMessage[20000];
//		_capacity = 20000;
//		_in = 0;
//		_ou = 0;
//		_cnt = 0;
//	}
//	void Push(icsSpyMessage msg)
//	{
//		std::lock_guard < std::recursive_timed_mutex > lock(_mutex);
//
//		_msgs[_in] = 
//	}
//};