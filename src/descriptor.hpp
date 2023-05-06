#pragma once

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

#include "simple_nimble_type.hpp"

namespace SimpleNimble {
struct Descriptor {
	ble_uuid_any_t uuid;
	Dsc_AccessFlag flag;
	uint8_t buffer[32];
	size_t buffer_size = 32;
	uint16_t data_length;
};
}  // namespace SimpleNimble
