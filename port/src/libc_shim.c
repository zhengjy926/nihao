/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * The Arm Compiler C library stdout hooks. The console driver that used to
 * install them is gone (see ADR-0006), so nothing calls them today; they stay
 * because this file is still the only fputc() implementation. Upstream's
 * version (lib/libc/armstdc/src/libc-hooks.c) also defines
 * __aeabi_errno_addr() in terms of the kernel's current thread, which this
 * runtime does not have; errno is provided by errno_shim.c instead.
 */

#include <stdio.h>

static int stdout_hook_default(int c)
{
	(void)c;

	return EOF;
}

static int (*stdout_hook)(int) = stdout_hook_default;

void __stdout_hook_install(int (*hook)(int c))
{
	stdout_hook = hook;
}

int fputc(int c, FILE *f)
{
	(void)f;

	return stdout_hook(c);
}
