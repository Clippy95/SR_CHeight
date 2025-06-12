#include "multi.h"
packet_def* Multi_game_packets = (packet_def*)0xE8B6A8;



multi_session* multi_session_get_active_session() {
	multi_session_base Active_Session_ptr = *(uintptr_t*)0x2528C14;

	return (multi_session*)Active_Session_ptr;
}

static const uintptr_t multi_io_send_to_all_reliable_addr = 0x814740;
int __declspec(naked) multi_io_send_to_all_reliable(multi_session* single_packet, net_data* data, int len) {
	__asm {
		push ebp
		mov ebp, esp
		sub esp, __LOCAL_SIZE


		mov edi, single_packet
		push len
		push data
		call multi_io_send_to_all_reliable_addr

		mov esp, ebp
		pop ebp
		ret
	}
}

void __cdecl multi_game_send_CHeight_value(float value)
{
	multi_session* active_session;
	unsigned __int16 packet_size;
	unsigned __int8 group_id;
	unsigned __int8 packet_data[512]{};

	packet_data[0] = 67;
	packet_size = 3;
	Multi_game_packets[17].group_id = 1;
	group_id = Multi_game_packets[17].group_id;
	if (Multi_game_packets[17].group_id)
		++packet_size;

	//print("name is %s\n", Multi_game_packets[17].packet_name);

	memcpy(&packet_data[packet_size], &value, sizeof(float));
	packet_size += sizeof(float);

	unsigned __int16 dlen = packet_size - 3;
	memcpy(&packet_data[1], &dlen, sizeof(dlen));

	active_session = multi_session_get_active_session();
	multi_io_send_to_all_reliable(active_session, packet_data, packet_size);
}

