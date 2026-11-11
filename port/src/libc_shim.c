/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * The Arm Compiler C library hooks that uart_console installs. Upstream's
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
