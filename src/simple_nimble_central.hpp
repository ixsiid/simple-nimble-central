#pragma once

#include <nimble/ble.h>
#include <host/ble_uuid.h>

#include <freertos/event_groups.h>

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

class SimpleNimbleCentral {
    private:
	static const EventBits_t EVENT_DONE		    = 1 << 0;
	static const EventBits_t EVENT_CONNECTION_FAILED = 1 << 1;
	static const EventBits_t EVENT_FOUND		    = 1 << 2;
	static const EventBits_t EVENT_NOT_CONNECTION    = 1 << 3;
	static const EventBits_t EVENT_FAILED		    = 1 << 20;

	static SimpleNimbleCentral *instance;
	static EventGroupHandle_t event_group;

	static int svc_chr_disced(uint16_t conn_handle, const struct ble_gatt_error *error,
						 const void *svc_chr, void *arg);
	static int blecent_gap_event(struct ble_gap_event *event, void *arg);

	SimpleNimbleCentral();
	uint16_t handle;
	uint16_t service_start_handle, service_end_handle;
	uint16_t characteristic_handle;
	SimpleNimbleCallback callback;

    public:
	static SimpleNimbleCentral *get_instance();
	void register_callback(SimpleNimbleCallback callback);
	bool connect(const ble_addr_t *address);
	int disconnect();
	int find_service(const ble_uuid_t *service, int timeout = 10000);
	int find_characteristic(const ble_uuid_t *characteristic, int timeout = 10000);
	int write(const uint8_t *value, size_t length);
};
