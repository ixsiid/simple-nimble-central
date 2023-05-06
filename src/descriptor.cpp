#include "descriptor.hpp"

using namespace SimpleNimble;
/*
const char *Descriptor::tag = "DescriptorBuffer";

Descriptor::Descriptor(
    ble_uuid_any_t uuid, size_t size, SimpleNimble::AccessFlag flag)
    : uuid(uuid),
	 data_length(0),
	 buffer_size(size),
	 buffer(new uint8_t[size]),
	 flag(flag) {
}

const uint8_t *Descriptor::read() { return buffer; }

void Descriptor::write(const uint8_t *data, uint8_t length) {
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len);
	this->data_length = cpy_len;
}
void Descriptor::write(const char *data) {
	size_t length	= strlen(data);
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len + 1);
	this->data_length = cpy_len;
}

void Descriptor::write_u8(uint8_t data) {
	buffer[0] = data;

	this->data_length = 1;
}

void Descriptor::write_u16(uint16_t data) {
	buffer[0] = (data >> 8) & 0xff;
	buffer[1] = (data >> 0) & 0xff;

	this->data_length = 2;
}

void Descriptor::write_u32(uint32_t data) {
	buffer[0] = (data >> 24) & 0xff;
	buffer[1] = (data >> 16) & 0xff;
	buffer[2] = (data >> 8) & 0xff;
	buffer[3] = (data >> 0) & 0xff;

	this->data_length = 4;
}

void Descriptor::notify() {}

Descriptor::~Descriptor() {
	delete[] buffer;
}

void Descriptor::create_def(struct ble_gatt_chr_def *ptr) {
	ptr->uuid		   = &uuid.u;
	ptr->access_cb	   = access_callback;
	ptr->arg		   = this;
	ptr->descriptors  = nullptr;
	ptr->flags	   = static_cast<int>(flag);
	ptr->min_key_size = 0;
	ptr->val_handle   = &val_handle;
}

int Descriptor::access_callback(uint16_t conn_handle,
						  uint16_t attr_handle,
						  struct ble_gatt_access_ctxt *ctxt,
						  void *arg) {
	int rc;
	Descriptor *s = (Descriptor *)arg;

	switch (ctxt->op) {
		case BLE_GATT_ACCESS_OP_READ_CHR:
			ESP_LOGI(tag, "read");
			if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
				MODLOG_DFLT(INFO, "Characteristic read; conn_handle=%d attr_handle=%d\n",
						  conn_handle, attr_handle);
			} else {
				MODLOG_DFLT(INFO, "Characteristic read by NimBLE stack; attr_handle=%d\n",
						  attr_handle);
			}
			if (attr_handle == s->val_handle) {
				rc = os_mbuf_append(ctxt->om,
								s->buffer,
								s->data_length);
				return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			break;

		case BLE_GATT_ACCESS_OP_WRITE_CHR:
			ESP_LOGI(tag, "write");
			if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
				MODLOG_DFLT(INFO, "Characteristic write; conn_handle=%d attr_handle=%d",
						  conn_handle, attr_handle);
			} else {
				MODLOG_DFLT(INFO, "Characteristic write by NimBLE stack; attr_handle=%d",
						  attr_handle);
			}

			if (attr_handle == s->val_handle) {
				rc = ble_hs_mbuf_to_flat(ctxt->om, s->buffer, s->buffer_size, &s->data_length);
				ble_gatts_chr_updated(attr_handle);
				MODLOG_DFLT(INFO,
						  "Notification/Indication scheduled for "
						  "all subscribed peers.\n");
				return rc;
			}
			break;
	}

	return BLE_ATT_ERR_UNLIKELY;
}
*/