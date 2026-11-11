/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * The upstream device model iterates the device list through these linker
 * symbols. scatter.sct brackets the device section with the two empty
 * sections used here.
 */

const char _device_list_start[0]
	__attribute__((section(".nihao_device_begin"), used, aligned(4)));
const char _device_list_end[0]
	__attribute__((section(".nihao_device_end"), used, aligned(4)));
