#include <esp_log.h>
#include <nvs_flash.h>

#include <host/ble_gap.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include <nimble/nimble_port.h>
#include <esp_nimble_hci.h>
#include <host/util/util.h>

#include "simple_nimble_central.hpp"

const char *SimpleNimbleCentral::tag = "SimpleNimbleCentral";

EventGroupHandle_t SimpleNimbleCentral::event_group = xEventGroupCreate();
SimpleNimbleCentral *SimpleNimbleCentral::instance  = nullptr;

SimpleNimbleCentral::SimpleNimbleCentral()
    : handle(0),
	 service_start_handle(0),
	 service_end_handle(0),
	 characteristic_handle(0),
	 callback([](NimbleCallbackReason reason) {}) {
	ESP_LOGI(tag, "create instance");

	// esp_nimble_hci_and_controller_init();
	nimble_port_init();
	nimble_port_freertos_init([](void *param) {
		ESP_LOGI(tag, "BLE Host Task Started");
		nimble_port_run();

		ESP_LOGI(tag, "nimble_port_freertos_deinit");
		nimble_port_freertos_deinit();
	});
}

enum class DiscType : uintptr_t {
	Service,
	Characteristic,
};

int SimpleNimbleCentral::svc_chr_disced(uint16_t conn_handle,
								const struct ble_gatt_error *error,
								const void *svc_chr, void *arg) {
	int rc = 0;
	ESP_LOGI(tag, "event status: %d", error->status);

	switch (error->status) {
		case 0: {
			DiscType mode = static_cast<DiscType>(reinterpret_cast<uintptr_t>(arg));
			switch (mode) {
				case DiscType::Service: {
					// find_service
					const struct ble_gatt_svc *service = static_cast<const struct ble_gatt_svc *>(svc_chr);

					instance->service_start_handle = service->start_handle;
					instance->service_end_handle	 = service->end_handle;
					break;
				}
				case DiscType::Characteristic: {
					// find_characteristic
					const struct ble_gatt_chr *chr = static_cast<const struct ble_gatt_chr *>(svc_chr);

					instance->characteristic_handle = chr->val_handle;
					break;
				}
			}
			xEventGroupSetBits(event_group, EVENT_FOUND);

			break;
		}
		// ENOTCONNが呼ばれたときに、EDONEが呼ばれないかは不明
		case BLE_HS_ENOTCONN:
			xEventGroupSetBits(event_group, EVENT_NOT_CONNECTION | EVENT_DONE);
			break;

		case BLE_HS_EDONE:
			xEventGroupSetBits(event_group, EVENT_DONE);
			break;
		default:
			rc = error->status;
			break;
	}

	return rc;
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
int SimpleNimbleCentral::blecent_gap_event(struct ble_gap_event *event, void *arg) {
	struct ble_gap_conn_desc desc;
	int rc;

	ESP_LOGV(tag, "blecent_gap_event: %d", event->type);

	switch (event->type) {
		case BLE_GAP_EVENT_CONNECT:
			/* A new connection was established or a connection attempt failed. */
			if (event->connect.status == 0) {
				/* Connection successfully established. */
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: success");

				// conn_handleが常に0なのが謎
				// 何かの拍子に non zero となった時にすぐ分かるようのアサーション
				// ble_gattc_disc_svc_by_uuid と ble_gap_terminate の引数に影響する
				assert(event->connect.conn_handle == 0);

				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				xEventGroupSetBits(event_group, EVENT_DONE);
			} else {
				/* Connection attempt failed; resume scanning. */
				// MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n", event->connect.status);
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: failed");

				xEventGroupSetBits(event_group, EVENT_CONNECTION_FAILED | EVENT_DONE);
			}

			return 0;
		case BLE_GAP_EVENT_REPEAT_PAIRING:
			/* We already have a bond with the peer, but it is attempting to
			 * establish a new secure link.  This app sacrifices security for
			 * convenience: just throw away the old bond and accept the new link.
			 */

			/* Delete the old bond. */
			rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
			assert(rc == 0);
			ble_store_util_delete_peer(&desc.peer_id_addr);

			/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
			 * continue with the pairing operation.
			 */
			return BLE_GAP_REPEAT_PAIRING_RETRY;

		case BLE_GAP_EVENT_DISCONNECT:
			ESP_LOGI(tag, "BLE_GAP_EVENT_DISCONNECT: %d", event->connect.conn_handle);
			instance->callback(NimbleCallbackReason::DISCONNECT);
			return 0;
		default:
			return 0;
	}
}

SimpleNimbleCentral *SimpleNimbleCentral::get_instance() {
	if (instance == nullptr) instance = new SimpleNimbleCentral();
	return instance;
}

void SimpleNimbleCentral::register_callback(SimpleNimbleCallback callback) {
	this->callback = callback;
}

bool SimpleNimbleCentral::connect(const ble_addr_t *address) {
	int rc;

	xEventGroupClearBits(event_group, 0xffffff);
	rc = ble_gap_connect(0, address, 4000, nullptr, blecent_gap_event, nullptr);
	ESP_LOGI(tag, "gap connect result: %d", rc);
	if (rc == BLE_HS_EDONE)
		return true;
	else if (rc)
		return false;

	EventBits_t b = xEventGroupWaitBits(event_group, EVENT_DONE, true, false, portMAX_DELAY);
	vTaskDelay(0);
	if (b & EVENT_CONNECTION_FAILED) return false;
	return true;
}

int SimpleNimbleCentral::disconnect() {
	ESP_LOGI(tag, "disconnect");
	// handleが常に0なのは謎
	ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
	return 0;
}

int SimpleNimbleCentral::find_service(const ble_uuid_t *service, int timeout) {
	xEventGroupClearBits(event_group, 0xffffff);

	// handleが常に0なのは謎
	ble_gattc_disc_svc_by_uuid(handle, service,
						  (int (*)(uint16_t, const struct ble_gatt_error *, const struct ble_gatt_svc *, void *))svc_chr_disced,
						  reinterpret_cast<void *>(DiscType::Service));

	EventBits_t b = xEventGroupWaitBits(event_group, EVENT_DONE, true, false, portMAX_DELAY);
	vTaskDelay(0);

	if (b &= EVENT_FOUND) return true;
	return false;
}

int SimpleNimbleCentral::find_characteristic(const ble_uuid_t *characteristic, int timeout) {
	xEventGroupClearBits(event_group, 0xffffff);

	// handleが常に0なのは謎
	ble_gattc_disc_chrs_by_uuid(handle,
						   service_start_handle,
						   service_end_handle,
						   characteristic,
						   (int (*)(uint16_t, const struct ble_gatt_error *, const struct ble_gatt_chr *, void *))svc_chr_disced,
						   reinterpret_cast<void *>(DiscType::Characteristic));

	EventBits_t b = xEventGroupWaitBits(event_group, EVENT_DONE, true, false, portMAX_DELAY);
	vTaskDelay(0);

	if (b &= EVENT_FOUND) return true;
	return false;
}

int SimpleNimbleCentral::write(const uint8_t *value, size_t length) {
	int rc = ble_gattc_write_no_rsp_flat(handle, characteristic_handle, value, length);
	if (rc) return false;
	return true;
}
