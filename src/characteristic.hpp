#pragma once

#include <initializer_list>
#include <functional>

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

#include "simple_nimble_type.hpp"

namespace SimpleNimble {
class Characteristic {
    private:
	static const char *tag;

	ble_uuid_any_t uuid;
	uint16_t data_length;
	size_t buffer_size;
	uint8_t *buffer;
	Chr_AccessFlag flag;
	uint16_t val_handle;
	ble_gatt_dsc_def *descriptors;

	std::function<void(NimbleCallbackReason)> callback;
	void *callback_param;

	static int access_callback(uint16_t conn_handle, uint16_t attr_handle,
						  struct ble_gatt_access_ctxt *ctxt, void *arg);

    public:
	Characteristic(uint32_t uuid16or32, size_t buffer_size, Chr_AccessFlag flag,
				std::initializer_list<Descriptor *> descriptors = {});
	~Characteristic();

	const uint8_t *read();
	void write(const uint8_t *data, uint8_t length);
	void write(std::initializer_list<uint8_t> data);
	void write(const char *data);
	void write_u8(uint8_t data);
	void write_u16(uint16_t data);
	void write_u32(uint32_t data);
	void clear(uint8_t size);
	void notify();

	void set_callback(std::function<void(NimbleCallbackReason)> callback);

	void create_def(struct ble_gatt_chr_def *ptr);
};
}  // namespace SimpleNimble