/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Upstream walks its logging lists through linker-provided start/end symbols,
 * and cbprintf decides whether a pointer may be dereferenced by checking it
 * against the read-only region. scatter.sct brackets every one of those areas
 * with the zero-length markers defined here.
 *
 * These symbols are only *defined* in this translation unit; the code that
 * indexes them lives elsewhere, because indexing a zero-length array in the
 * unit that defines it lets the compiler delete the loop.
 */

const char _log_const_list_start[0] __attribute__((section(".nihao_log_const_begin"), used, aligned(4)));
const char _log_const_list_end[0] __attribute__((section(".nihao_log_const_end"), used, aligned(4)));
const char _log_backend_list_start[0] __attribute__((section(".nihao_log_backend_begin"), used, aligned(4)));
const char _log_backend_list_end[0] __attribute__((section(".nihao_log_backend_end"), used, aligned(4)));
const char _log_msg_ptr_list_start[0] __attribute__((section(".nihao_log_msg_ptr_begin"), used, aligned(4)));
const char _log_msg_ptr_list_end[0] __attribute__((section(".nihao_log_msg_ptr_end"), used, aligned(4)));
const char _log_dynamic_list_start[0] __attribute__((section(".nihao_log_dynamic_begin"), used, aligned(4)));
const char _log_dynamic_list_end[0] __attribute__((section(".nihao_log_dynamic_end"), used, aligned(4)));
const char _log_link_list_start[0] __attribute__((section(".nihao_log_link_begin"), used, aligned(4)));
const char _log_link_list_end[0] __attribute__((section(".nihao_log_link_end"), used, aligned(4)));

/* Interrupt registrations collected by IRQ_CONNECT() (see ADR-0004). */
const char _intlist_list_start[0] __attribute__((section(".nihao_intlist_begin"), used, aligned(4)));
const char _intlist_list_end[0] __attribute__((section(".nihao_intlist_end"), used, aligned(4)));
const char _log_mpsc_pbuf_list_start[0] __attribute__((section(".nihao_log_mpsc_pbuf_begin"), used, aligned(4)));
const char _log_mpsc_pbuf_list_end[0] __attribute__((section(".nihao_log_mpsc_pbuf_end"), used, aligned(4)));