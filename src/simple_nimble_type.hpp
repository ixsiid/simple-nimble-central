#pragma once

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

namespace SimpleNimble {
enum class NimbleCallbackReason {
	UNKNOWN = 0,
	SUCCESS,
	DONE,
	CHARACTERISTIC_WRITE_FAILED,
	CHARACTERISTIC_FIND_FAILED,
	SERVICE_FIND_FAILED,
	STOP_CANCEL_FAILED,
	CONNECTION_FAILED,
	OTHER_FAILED,
	// 途中経過
	CONNECTION_START,
	CONNECTION_ESTABLISHED,
	DISCONNECT,
};

typedef void (*SimpleNimbleCallback)(NimbleCallbackReason);

enum class Chr_AccessFlag {  // for characteristic
	Read			   = BLE_GATT_CHR_F_READ,
	Write_no_response = BLE_GATT_CHR_F_WRITE_NO_RSP,
	Write		   = BLE_GATT_CHR_F_WRITE,
	Notify		   = BLE_GATT_CHR_F_NOTIFY,
};

enum class Dsc_AccessFlag {  // for descriptor
	Read	 = BLE_ATT_F_READ,
	Write = BLE_ATT_F_WRITE,
};

constexpr Chr_AccessFlag operator|(Chr_AccessFlag l, Chr_AccessFlag r) {
	return static_cast<Chr_AccessFlag>(static_cast<int>(l) | static_cast<int>(r));
}
constexpr Dsc_AccessFlag operator|(Dsc_AccessFlag l, Dsc_AccessFlag r) {
	return static_cast<Dsc_AccessFlag>(static_cast<int>(l) | static_cast<int>(r));
}

const uint8_t descriptor_buffer_size = 16;
struct Descriptor {
	const ble_uuid_t * uuid;
	Dsc_AccessFlag flag;
	uint16_t data_length;
	uint8_t buffer[descriptor_buffer_size];
};
}  // namespace SimpleNimble
