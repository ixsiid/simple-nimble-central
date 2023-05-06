#pragma once

#include <initializer_list>

#include <nimble/ble.h>
#include <host/ble_uuid.h>

#include <freertos/event_groups.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <services/ans/ble_svc_ans.h>

#include "simple_nimble_type.hpp"
#include "service.hpp"

using namespace SimpleNimble;

class SimpleNimblePeripheral {
    private:
	static const char *tag;

	static SimpleNimblePeripheral *instance;
	static EventGroupHandle_t event_group;

	static ble_gap_adv_params adv_params;
	static int ble_gap_event(struct ble_gap_event *event, void *arg);

	SimpleNimblePeripheral();
	~SimpleNimblePeripheral();
	SimpleNimbleCallback callback;

	uint8_t service_length;
	uint8_t registered_service_count;

	ble_gatt_svc_def *services;
	ble_hs_adv_fields fields;
	ble_uuid_any_t *service_uuids;

	void start_advertise();

	static void print_services(SimpleNimblePeripheral *obj);

    public:
	static SimpleNimblePeripheral *get_instance();

	void set_name(const char *name);

	void initialize_services(uint8_t service_count);
	bool add_service(SimpleNimbleCallback callback, Service *service);

	void start();

	void debug();
};
