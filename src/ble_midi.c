/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>

#include "midi_service.h"
//#include "pico/btstack_cyw43.h"
#include "btstack.h"

// clang-format off
static uint8_t ble_advertising_data[] = {
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    5, BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME, 'P', 'i', 'c', 'o',
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
        0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03,
};
// clang-format on

typedef enum {
    GAP_DEVICE_NAME_HANDLE = ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE,
    MIDI_NOTE_HANDLE = ATT_CHARACTERISTIC_7772E5DB_3868_4112_A1A9_F2669D106BF3_01_VALUE_HANDLE,
} attribute_handle_t;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static btstack_timer_source_t step_timer;

void send_midi_note(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (con_handle == HCI_CON_HANDLE_INVALID)
        return;

    uint8_t packet[] = {0x80,     0x80, (uint8_t)(0x90 | (channel & 0x0F)), note,
                        velocity, 0x80, (uint8_t)(0x80 | (channel & 0x0F)), note,
                        0x00};
    att_server_notify(con_handle, MIDI_NOTE_HANDLE, packet, sizeof(packet));
}

bool ble_midi_connection_status(void) {
    return con_handle != HCI_CON_HANDLE_INVALID;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET)
        return;
    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
                return;
            uint16_t adv_int_min = 800;
            uint16_t adv_int_max = 800;
            uint8_t adv_type = 0;
            bd_addr_t null_addr;
            memset(null_addr, 0, 6);
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07,
                                          0x00);
            gap_advertisements_set_data(sizeof(ble_advertising_data),
                                        (uint8_t *)ble_advertising_data);
            gap_advertisements_enable(1);
            break;
        case HCI_EVENT_LE_META:
            uint8_t subevent = hci_event_le_meta_get_subevent_code(packet);
            if (subevent == HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                printf("[BLE] Connection established, handle: 0x%04x\n", con_handle);
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            printf("[BLE] Disconnected, handle: 0x%04x\n",
                   hci_event_disconnection_complete_get_connection_handle(packet));
            break;
        default:
            break;
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle,
                                  uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    (void)connection_handle;
    if (att_handle == GAP_DEVICE_NAME_HANDLE) {
        bd_addr_t local_addr;
        gap_local_bd_addr(local_addr);
        const char *mac = bd_addr_to_str(local_addr);
        char device_name[] = "Pico 00:00:00:00:00:00";
        memcpy(device_name + 5, mac, strlen(mac));
        return att_read_callback_handle_blob((const uint8_t *)device_name, strlen(device_name),
                                             offset, buffer, buffer_size);
    }
    return 0;
}

void ble_midi_init(void (*step_cb)(btstack_timer_source_t *ts), uint32_t step_period_ms) {
    l2cap_init();
    sm_init();
    att_server_init(profile_data, att_read_callback, NULL);
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(packet_handler);

    btstack_run_loop_set_timer_handler(&step_timer, step_cb);
    btstack_run_loop_set_timer(&step_timer, step_period_ms);
    btstack_run_loop_add_timer(&step_timer);

    hci_power_control(HCI_POWER_ON);
}
