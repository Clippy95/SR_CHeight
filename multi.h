#pragma once
#include "pch.h"
#include <windows.h>
#include <safetyhook.hpp>
#include "SRMath.h"
typedef uint32_t multi_session_base;
typedef uint32_t multi_session;
typedef uint32_t multi_connection;
typedef unsigned __int8 net_data;
typedef unsigned __int8 net_data;
#define USE_CON 0

#define DBG USE_CON || _DEBUG

#if _DEBUG || USE_CON
#define print(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define print(fmt, ...) ((void)0)
#endif


struct sender
{
	uintptr_t* address;
	uintptr_t* connection;
};
multi_session* multi_session_get_active_session();
int multi_io_send_to_all_reliable(multi_session* single_packet, net_data* data, int len);

enum packet_direction : __int32
{
	PD_SERVER_TO_CLIENT = 0x0,
	PD_CLIENT_TO_SERVER = 0x1,
	PD_BOTH = 0x2,
};
void __cdecl multi_game_send_CHeight_value(float value);


struct __declspec(align(4)) packet_def
{
	unsigned __int8 packet_id;
	void(__cdecl* receive_func)(unsigned __int8*, const sender*);
	char* packet_name;
	packet_direction direction;
	unsigned __int16 valid_netinfo_states;
	bool connection_required;
	bool can_receive_blind;
	unsigned __int8 group_id;
};
extern packet_def* Multi_game_packets;