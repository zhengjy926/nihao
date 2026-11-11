/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * errno plumbing for the Arm Compiler C library without a Zephyr kernel.
 */

#include <errno.h>
#include <zephyr/init.h>

static int port_errno;

int *z_impl_z_errno(void)
{
	return &port_errno;
}

volatile int *__aeabi_errno_addr(void)
{
	return &port_errno;
}
