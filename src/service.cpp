#include "service.hpp"

#include <host/ble_gatt.h>

using namespace SimpleNimble;

const char *Service::tag = "Service";

Service::Service(uint32_t uuid16or32, size_t characteristic_count)
    : characteristic_count(characteristic_count),
	 characteristic_index(new ble_gatt_chr_def[characteristic_count + 1]{}),
	 characteristics(characteristic_index) {
	ESP_LOGI(tag, "create service instance");
	uuid.u.type = uuid16or32 & 0xffff0000 ? BLE_UUID_TYPE_32 : BLE_UUID_TYPE_16;
	if (uuid.u.type == BLE_UUID_TYPE_16) {
		uuid.u16.value = (uint16_t)uuid16or32;
	} else if (uuid.u.type == BLE_UUID_TYPE_32) {
		uuid.u32.value = uuid16or32;
	}

	ESP_LOG_BUFFER_HEXDUMP(tag, characteristics, sizeof(ble_gatt_chr_def) * (characteristic_count + 1), esp_log_level_t::ESP_LOG_INFO);
}

Service::~Service() {
	if (characteristics) delete[] characteristics;
}

void Service::add_characteristic(Characteristic *characteristic) {
	characteristic->create_def(characteristic_index++);
}

void Service::create_def(ble_gatt_svc_def *out) {
	out->type			 = BLE_GATT_SVC_TYPE_PRIMARY;
	out->uuid			 = &uuid.u;
	out->includes		 = nullptr;
	out->characteristics = characteristics;
}

uint Service::type() { return uuid.u.type; }