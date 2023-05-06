#pragma once

#include <initializer_list>

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

#include "simple_nimble_type.hpp"
#include "characteristic.hpp"

namespace SimpleNimble {
class Service {
    private:
	static const char *tag;

	ble_uuid_any_t uuid;

	size_t characteristic_count;
	ble_gatt_chr_def *characteristic_index;
	const ble_gatt_chr_def *characteristics;

    public:
	Service(uint32_t uuid16or32, size_t characteristic_count = 0);
	~Service();

	void add_characteristic(Characteristic *Characteristic);
	void create_def(ble_gatt_svc_def *out);
	uint type();
};
}  // namespace SimpleNimble