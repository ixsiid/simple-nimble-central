#include "characteristic.hpp"

const char *SimpleNimbleCharacteristicBuffer::tag = "CharacteristicBuffer";

SimpleNimbleCharacteristicBuffer::SimpleNimbleCharacteristicBuffer(ble_uuid_any_t uuid, uint8_t size, Flag flag)
    : uuid(uuid), length(size), buffer(new uint8_t[size]), flag(flag) {
}

const uint8_t *SimpleNimbleCharacteristicBuffer::read() { return buffer; }

void SimpleNimbleCharacteristicBuffer::write(const uint8_t *data, uint8_t length) {
	memcpy(buffer, data, length);
}

void SimpleNimbleCharacteristicBuffer::write_u8(uint8_t data) {
	buffer[0] = data;
}

void SimpleNimbleCharacteristicBuffer::write_u16(uint16_t data) {
	buffer[0] = (data >> 8) & 0xff;
	buffer[1] = (data >> 0) & 0xff;
}

void SimpleNimbleCharacteristicBuffer::write_u32(uint32_t data) {
	buffer[0] = (data >> 24) & 0xff;
	buffer[1] = (data >> 16) & 0xff;
	buffer[2] = (data >> 8) & 0xff;
	buffer[3] = (data >> 0) & 0xff;
}

void SimpleNimbleCharacteristicBuffer::notify() {}

SimpleNimbleCharacteristicBuffer::~SimpleNimbleCharacteristicBuffer() {
	delete[] buffer;
}

void SimpleNimbleCharacteristicBuffer::create_def(struct ble_gatt_chr_def *ptr) {
	ptr->uuid		   = &uuid.u;
	ptr->access_cb	   = access_callback;
	ptr->arg		   = this;
	ptr->descriptors  = nullptr;
	ptr->flags	   = static_cast<int>(flag);
	ptr->min_key_size = 0;
	ptr->val_handle   = &val_handle;
}

int SimpleNimbleCharacteristicBuffer::access_callback(uint16_t conn_handle, uint16_t attr_handle,
										    struct ble_gatt_access_ctxt *ctxt, void *arg) {
	ESP_LOGI(tag, "%d, %d", conn_handle, attr_handle);
	return BLE_ATT_ERR_UNLIKELY;
}
