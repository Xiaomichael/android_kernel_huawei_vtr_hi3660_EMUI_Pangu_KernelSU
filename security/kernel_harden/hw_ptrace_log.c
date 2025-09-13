/*
 * Huawei Kernel Harden, ptrace log upload
 *
 * Copyright (c) 2016 Huawei.
 *
 * Authors:
 * yinyouzhan <yinyouzhan@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/capability.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/ptrace.h>
#include <linux/security.h>
#include <linux/signal.h>
#include <linux/uio.h>
#include <linux/audit.h>
#include <linux/pid_namespace.h>
#include <chipset_common/security/hw_kernel_stp_interface.h>

void record_ptrace_info_before_return(long request, struct task_struct *child)
{
	struct task_struct *tracer;
	char tcomm_child[TASK_COMM_LEN] = {0}; // 修改为固定大小 TASK_COMM_LEN
	char tcomm_tracer[TASK_COMM_LEN] = {0}; // 修改为固定大小 TASK_COMM_LEN
	static unsigned int  g_ptrace_log_counter = 0;

	if (child == NULL)
		return;

	if (g_ptrace_log_counter >= 100) /* only 100 log upload since power on */
		return;
	g_ptrace_log_counter++;

	(void)get_task_comm(tcomm_child, child);
	rcu_read_lock();
	tracer = ptrace_parent(child);
	if (tracer) {
		(void)get_task_comm(tcomm_tracer, tracer);
	} else {
		// 使用更安全的方式复制字符串
		strncpy(tcomm_tracer, "unknown", TASK_COMM_LEN - 1);
		tcomm_tracer[TASK_COMM_LEN - 1] = '\0'; // 确保字符串以 null 结尾
	}
	rcu_read_unlock();
	struct stp_item item;
	// 调整 add_info 的大小以适应新的缓冲区大小
	char add_info[TASK_COMM_LEN * 2 + 1] = {0}; // 两个 TASK_COMM_LEN 加上一个结束符
	(void)memset(&item, 0, sizeof(item));
	item.id = item_info[PTRACE].id;
	item.status = STP_RISK;
	item.credible = STP_CREDIBLE;
	item.version = 0;
	(void)strncpy(item.name, item_info[PTRACE].name, STP_ITEM_NAME_LEN - 1);
	// 确保 snprintf 不会溢出
	(void)snprintf(add_info, sizeof(add_info) - 1, "%s%s", tcomm_child, tcomm_tracer);
	int ret = kernel_stp_upload(item, add_info);
	if (ret != 0) {
		pr_err("stp ptrace upload fail, child_cmdline=%s, tracer_cmdline=%s\n", tcomm_child, tcomm_tracer);
	}
	else {
		pr_err("stp ptrace upload succ, child_cmdline=%s, tracer_cmdline=%s\n", tcomm_child, tcomm_tracer);
	}

	return;
}