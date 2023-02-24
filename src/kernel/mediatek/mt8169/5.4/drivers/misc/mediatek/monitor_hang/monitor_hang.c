// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/poll.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <uapi/linux/sched/types.h>

#include <mt-plat/aee.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mboot_params.h>
#endif
#include <mt-plat/mrdump.h>

#include "aed/aed.h"
#include "monitor_hang.h"
#include "mrdump/mrdump_private.h"
#include "mrdump/mrdump_mini.h"

#ifndef TASK_STATE_TO_CHAR_STR
#define TASK_STATE_TO_CHAR_STR "RSDTtZXxKWPNn"
#endif

#ifdef CONFIG_MTK_HANG_DETECT_DB
#define MAX_HANG_INFO_SIZE (2*1024*1024) /* 2M info */
#define MAX_STRING_SIZE 256
#define MEM_BUFFER_DEFAULT_SIZE (3*1024)
#define MSDC_BUFFER_DEFAULT_SIZE (30*1024)
static int MaxHangInfoSize = MAX_HANG_INFO_SIZE;
static char *Hang_Info;
static int Hang_Info_Size;
static bool watchdog_thread_exist;
static bool system_server_exist;
#endif

static bool Hang_Detect_first;
static bool hd_detect_enabled;
static int hd_timeout = 0x7fffffff;
static int hang_detect_counter = 0x7fffffff;
static int dump_bt_done;
static bool reboot_flag;
static struct name_list *white_list;



DECLARE_WAIT_QUEUE_HEAD(dump_bt_start_wait);
DECLARE_WAIT_QUEUE_HEAD(dump_bt_done_wait);
DEFINE_RAW_SPINLOCK(white_list_lock);

static void show_status(int flag);
static void monitor_hang_kick(int lParam);

static void reset_hang_info(void)
{
	Hang_Detect_first = false;
}

int add_white_list(char *name)
{
	struct name_list *new_thread;
	struct name_list *pList;

	raw_spin_lock(&white_list_lock);
	if (!white_list) {
		new_thread = kmalloc(sizeof(struct name_list), GFP_KERNEL);
		if (!new_thread) {
			raw_spin_unlock(&white_list_lock);
			return -1;
		}
		strncpy(new_thread->name, name, TASK_COMM_LEN);
		new_thread->name[TASK_COMM_LEN - 1] = 0;
		new_thread->next = NULL;
		white_list = new_thread;
		raw_spin_unlock(&white_list_lock);
		return 0;
	}

	pList = white_list;
	while (pList) {
		/*find same thread name*/
		if (strncmp(pList->name, name, TASK_COMM_LEN) == 0) {
			raw_spin_unlock(&white_list_lock);
			return 0;
		}
		pList = pList->next;
	}

	/*add new thread name*/
	new_thread = kmalloc(sizeof(struct name_list), GFP_KERNEL);
	if (!new_thread) {
		raw_spin_unlock(&white_list_lock);
		return -1;
	}

	strncpy(new_thread->name, name, TASK_COMM_LEN);
	new_thread->next = white_list;
	white_list = new_thread;
	raw_spin_unlock(&white_list_lock);
	return 0;
}

int del_white_list(char *name)
{
	struct name_list *pList;
	struct name_list *pList_old;

	if (!white_list)
		return 0;

	raw_spin_lock(&white_list_lock);
	pList = pList_old = white_list;
	while (pList) {
		/*find same thread name*/
		if (strncmp(pList->name, name, TASK_COMM_LEN) == 0) {
			if (pList == white_list) {
				white_list = pList->next;
				kfree(pList);
				raw_spin_unlock(&white_list_lock);
				return 0;
			}

			pList_old->next = pList->next;
			kfree(pList);
			raw_spin_unlock(&white_list_lock);
			return 0;
		}
		pList_old = pList;
		pList = pList->next;
	}
	raw_spin_unlock(&white_list_lock);
	return 0;
}

/******************************************************************************
 * hang detect File operations
 *****************************************************************************/
static int monitor_hang_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int monitor_hang_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int monitor_hang_poll(struct file *file,
		struct poll_table_struct *ptable)
{
	return 0;
}

static ssize_t monitor_hang_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t monitor_hang_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}

static long monitor_hang_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	static long long monitor_status;
	void __user *argp = (void __user *)arg;
	char name[TASK_COMM_LEN];

	if (cmd == HANG_KICK) {
		pr_info("hang_detect HANG_KICK ( %d)\n", (int)arg);
		monitor_hang_kick((int)arg);
		return ret;
	}

	if ((cmd == HANG_SET_SF_STATE) &&
		(!strncmp(current->comm, "surfaceflinger", 10) ||
		!strncmp(current->comm, "SWWatchDog", 10))) {
		if (copy_from_user(&monitor_status, argp, sizeof(long long)))
			ret = -EFAULT;
		return ret;
	} else if (cmd == HANG_GET_SF_STATE) {
		if (copy_to_user(argp, &monitor_status, sizeof(long long)))
			ret = -EFAULT;
		return ret;
	}

#ifdef CONFIG_MTK_HANG_DETECT_DB
	if (cmd == HANG_SET_REBOOT) {
		reboot_flag = true;
		hang_detect_counter = 2;
		hd_timeout = 3;
		hd_detect_enabled = true;
		pr_info("hang_detect: %s set reboot command.\n", current->comm);
		return ret;
	}
#endif

	if (cmd == HANG_ADD_WHITE_LIST) {
		if (copy_from_user(name, argp, TASK_COMM_LEN))
			ret = -EFAULT;
		ret = add_white_list(name);
		pr_info("hang_detect: add white list %s status %d.\n",
			name, ret);
		return ret;
	}

	if (cmd == HANG_DEL_WHITE_LIST) {
		if (copy_from_user(name, argp, TASK_COMM_LEN))
			ret = -EFAULT;
		ret = del_white_list(name);
		pr_info("hang_detect: del white list %s status %d.\n",
			name, ret);
		return ret;
	}

	return ret;
}


static const struct file_operations Hang_Monitor_fops = {
	.owner = THIS_MODULE,
	.open = monitor_hang_open,
	.release = monitor_hang_release,
	.poll = monitor_hang_poll,
	.read = monitor_hang_read,
	.write = monitor_hang_write,
	.unlocked_ioctl = monitor_hang_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = monitor_hang_ioctl,
#endif
};

static struct miscdevice Hang_Monitor_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "RT_Monitor",
	.fops = &Hang_Monitor_fops,
};

static int find_task_by_name(char *name)
{
	struct task_struct *task;
	int ret = -1;

	if (!name)
		return ret;
	rcu_read_lock();
	for_each_process(task) {
		if (task && !strncmp(task->comm, name, strlen(name))) {
			pr_info("[Hang_Detect] %s found pid:%d.\n",
					task->comm, task->pid);
			ret = task->pid;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}

static void log_hang_info(const char *fmt, ...)
{
#ifdef CONFIG_MTK_HANG_DETECT_DB
	unsigned long len;
	va_list ap;

	if ((Hang_Info_Size + MAX_STRING_SIZE) >=
			(unsigned long)MaxHangInfoSize)
		return;

	va_start(ap, fmt);
	len = vscnprintf(&Hang_Info[Hang_Info_Size], MAX_STRING_SIZE, fmt, ap);
	va_end(ap);
	Hang_Info_Size += len;
#endif
}

#ifdef CONFIG_MTK_HANG_DETECT_DB
#ifndef MODULE
static void buffer_hang_info(const char *buff, unsigned long size)
{
	if (((unsigned long)Hang_Info_Size + size)
		>= (unsigned long)MaxHangInfoSize)
		return;

	memcpy(&Hang_Info[Hang_Info_Size], buff, size);
	Hang_Info_Size += size;
}

static void dump_msdc_hang_info(void)
{
	char *buff_add = NULL;
	unsigned long buff_size = 0;

	if (get_msdc_aee_buffer) {
		get_msdc_aee_buffer((unsigned long *)&buff_add, &buff_size);
		if (buff_size != 0 && buff_add) {
			if (buff_size > MSDC_BUFFER_DEFAULT_SIZE) {
				buff_add = buff_add + buff_size - MSDC_BUFFER_DEFAULT_SIZE;
				buff_size = MSDC_BUFFER_DEFAULT_SIZE;
			}
			buffer_hang_info(buff_add, buff_size);
		}
	}
}

static void dump_mem_info(void)
{
	char *buff_add = NULL;
	int buff_size = 0;

	if (mlog_get_buffer) {
		mlog_get_buffer(&buff_add, &buff_size);
		if (buff_size <= 0 || !buff_add) {
			pr_info("hang_detect: mlog_get_buffer size %d.\n",
				buff_size);
			return;
		}

		if (buff_size > MEM_BUFFER_DEFAULT_SIZE) {
			buff_add = buff_add + buff_size - MEM_BUFFER_DEFAULT_SIZE;
			buff_size = MEM_BUFFER_DEFAULT_SIZE;
		}

		buffer_hang_info(buff_add, buff_size);
	}
}
#endif

void trigger_hang_db(void)
{
	pr_notice("[Hang_Detect] we  triger DB.\n");

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_hang_detect_timeout_count(hd_timeout);
	if ((!watchdog_thread_exist & system_server_exist)
		&& reboot_flag == false)
		aee_rr_rec_hang_detect_timeout_count(COUNT_ANDROID_REBOOT);
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		mrdump_mini_add_hang_raw((unsigned long)Hang_Info,
			MaxHangInfoSize);
		mrdump_mini_add_extra_misc();
		mrdump_common_die(AEE_REBOOT_MODE_HANG_DETECT,
		"	Hang Detect", NULL);
#else
		mrdump_common_die(AEE_REBOOT_MODE_HANG_DETECT,
			"Hang Detect", NULL);
#endif

}
#endif


#ifdef CONFIG_STACKTRACE

static void get_kernel_bt(struct task_struct *tsk)
{
	struct stack_trace trace;
	unsigned long stacks[32];
	int i;

	trace.entries = stacks;
	/*save backtraces */
	trace.nr_entries = 0;
	trace.max_entries = 32;
	trace.skip = 0;
	save_stack_trace_tsk(tsk, &trace);
	for (i = 0; i < trace.nr_entries; i++) {
		log_hang_info("<%lx> %pS\n", (long)trace.entries[i],
				(void *)trace.entries[i]);
		hang_log("<%lx> %pS\n", (long)trace.entries[i],
				(void *)trace.entries[i]);
	}
}

#endif

static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

void show_thread_info(struct task_struct *p, bool dump_bt)
{
	unsigned int state;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	state = p->state ? __ffs(p->state) + 1 : 0;

	log_hang_info("%-15.15s %c ", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	hang_log("%-15.15s %c ", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	log_hang_info("%lld.%06ld %d %lu %lu 0x%x 0x%lx %d ",
		nsec_high(p->se.sum_exec_runtime),
		nsec_low(p->se.sum_exec_runtime),
		task_pid_nr(p), p->nvcsw, p->nivcsw, p->flags,
		(unsigned long)task_thread_info(p)->flags,
		p->tgid);
	hang_log("%lld.%06ld %d %lu %lu 0x%x 0x%lx %d ",
		nsec_high(p->se.sum_exec_runtime),
		nsec_low(p->se.sum_exec_runtime),
		task_pid_nr(p), p->nvcsw, p->nivcsw, p->flags,
		(unsigned long)task_thread_info(p)->flags,
		p->tgid);
#ifdef CONFIG_SCHED_INFO
	log_hang_info("%llu", p->sched_info.last_arrival);
	hang_log("%llu", p->sched_info.last_arrival);
#endif
	log_hang_info("\n");

	/* nvscw: voluntary context switch.  */
	/* requires a resource that is unavailable. */
	/* nivcsw: involuntary context switch. */
	/* time slice out or when higher-priority thread to run*/
#ifdef	CONFIG_MTK_HANG_DETECT_DB
	if (!strcmp(p->comm, "watchdog"))
		watchdog_thread_exist = true;
	if (!strcmp(p->comm, "system_server"))
		system_server_exist = true;
#endif

#ifdef CONFIG_STACKTRACE
	if (dump_bt || ((p->state == TASK_RUNNING ||
			p->state & TASK_UNINTERRUPTIBLE) &&
			!strstr(p->comm, "wdtk")))
	/* Catch kernel-space backtrace */
		get_kernel_bt(p);
#endif
}

static int dump_native_maps(pid_t pid, struct task_struct *current_task)
{
	struct vm_area_struct *vma;
	int mapcount = 0;
	struct file *file;
	int flags;
	struct mm_struct *mm;
	struct pt_regs *user_ret;
	char tpath[512];
	char *path_p = NULL;
	struct path base_path;
	unsigned long long pgoff = 0;

	if (!current_task)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s: in user_mode", __func__, pid,
				current_task->comm);
		return -1;
	}

	if (!current_task->mm) {
		pr_info(" %s,%d:%s: current_task->mm == NULL", __func__, pid,
				current_task->comm);
		return -1;
	}

	vma = current_task->mm->mmap;
	log_hang_info("Dump native maps files:\n");
	hang_log("Dump native maps files:\n");
	while (vma && (mapcount < current_task->mm->map_count)) {
		file = vma->vm_file;
		flags = vma->vm_flags;
		pgoff = ((loff_t)vma->vm_pgoff) << PAGE_SHIFT;
		if (file) {	/* !!!!!!!!only dump 1st mmaps!!!!!!!!!!!! */
			if (flags & VM_EXEC) {
				/* we only catch code section for reduce
				 * maps space
				 */
				base_path = file->f_path;
				path_p = d_path(&base_path, tpath, 512);
				log_hang_info("%08lx-%08lx %c%c%c%c %08llx %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p',
					pgoff, path_p);
				hang_log("%08lx-%08lx %c%c%c%c %08llx %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p',
					pgoff, path_p);
			}
		} else {
			const char *name = aee_arch_vma_name(vma);

			mm = vma->vm_mm;
			if (!name) {
				if (mm) {
					if (vma->vm_start <= mm->start_brk &&
					    vma->vm_end >= mm->brk) {
						name = "[heap]";
					} else if (vma->vm_start <=
							mm->start_stack &&
						   vma->vm_end >=
							mm->start_stack) {
						name = "[stack]";
					}
				} else {
					name = "[vdso]";
				}
			}

			if (flags & VM_EXEC) {
				log_hang_info("%08lx-%08lx %c%c%c%c %08llx %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p', pgoff, name);
				hang_log("%08lx-%08lx %c%c%c%c %08llx %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p', pgoff, name);
			}
		}
		vma = vma->vm_next;
		mapcount++;
	}

	return 0;
}



static int dump_native_info_by_tid(pid_t tid,
	struct task_struct *current_task)
{
	struct pt_regs *user_ret;
	struct vm_area_struct *vma;
	unsigned long userstack_start = 0;
	unsigned long userstack_end = 0, length = 0;
	int ret = -1;

	if (!current_task)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s,fail in user_mode", __func__, tid,
				current_task->comm);
		return ret;
	}

	if (!current_task->mm) {
		pr_info(" %s,%d:%s, current_task->mm == NULL", __func__, tid,
				current_task->comm);
		return ret;
	}
#ifndef __aarch64__		/* 32bit */
	log_hang_info(" pc/lr/sp 0x%08x/0x%08x/0x%08x\n", user_ret->ARM_pc,
			user_ret->ARM_lr, user_ret->ARM_sp);
	hang_log(" pc/lr/sp 0x%08x/0x%08x/0x%08x\n", user_ret->ARM_pc,
				user_ret->ARM_lr, user_ret->ARM_sp);
	log_hang_info("r12-r0 0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_ip), (long)(user_ret->ARM_fp),
		(long)(user_ret->ARM_r10), (long)(user_ret->ARM_r9));
	hang_log("r12-r0 0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_ip), (long)(user_ret->ARM_fp),
		(long)(user_ret->ARM_r10), (long)(user_ret->ARM_r9));
	log_hang_info("0x%08x/0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_r8), (long)(user_ret->ARM_r7),
		(long)(user_ret->ARM_r6), (long)(user_ret->ARM_r5),
		(long)(user_ret->ARM_r4));
	hang_log("0x%08x/0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_r8), (long)(user_ret->ARM_r7),
		(long)(user_ret->ARM_r6), (long)(user_ret->ARM_r5),
		(long)(user_ret->ARM_r4));
	log_hang_info("0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_r3), (long)(user_ret->ARM_r2),
		(long)(user_ret->ARM_r1), (long)(user_ret->ARM_r0));
	hang_log("0x%08x/0x%08x/0x%08x/0x%08x\n",
		(long)(user_ret->ARM_r3), (long)(user_ret->ARM_r2),
		(long)(user_ret->ARM_r1), (long)(user_ret->ARM_r0));

	userstack_start = (unsigned long)user_ret->ARM_sp;

	vma = current_task->mm->mmap;
	while (vma) {
		if (vma->vm_start <= userstack_start &&
			vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == current_task->mm->mmap)
			break;
	}

	if (!userstack_end) {
		pr_info(" %s,%d:%s,userstack_end == 0", __func__,
				tid, current_task->comm);
		return ret;
	}
	length = userstack_end - userstack_start;


	/* dump native stack to buffer */
	{
		unsigned long SPStart = 0, SPEnd = 0;
		int tempSpContent[4], copied;

		SPStart = userstack_start;
		SPEnd = SPStart + length;
		log_hang_info("UserSP_start:%08x,Length:%08x,End:%08x\n",
				SPStart, length, SPEnd);
		hang_log("UserSP_start:%08x,Length:%08x,End:%08x\n",
				SPStart, length, SPEnd);
		while (SPStart < SPEnd) {
			copied =
			    access_process_vm(current_task, SPStart,
					&tempSpContent, sizeof(tempSpContent),
					0);
			if (copied != sizeof(tempSpContent)) {
				pr_info("access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n",
				  (unsigned int)sizeof(tempSpContent));
				/* return -EIO; */
			}
			if (tempSpContent[0] != 0 ||
				tempSpContent[1] != 0 ||
				tempSpContent[2] != 0 ||
				tempSpContent[3] != 0) {
				log_hang_info("%08x:%08x %08x %08x %08x\n", SPStart,
						tempSpContent[0],
						tempSpContent[1],
						tempSpContent[2],
						tempSpContent[3]);
				hang_log("%08x:%08x %08x %08x %08x\n", SPStart,
						tempSpContent[0],
						tempSpContent[1],
						tempSpContent[2],
						tempSpContent[3]);
			}
			SPStart += 4 * 4;
		}
	}
#else	/* 64bit, First deal with K64+U64, the last time to deal with K64+U32 */
	/* K64_U32 for current task */
	if (compat_user_mode(user_ret)) {	/* K64_U32 for check reg */
		log_hang_info("K64+ U32 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
			(long)(user_ret->user_regs.pc),
			(long)(user_ret->user_regs.regs[14]),
			(long)(user_ret->user_regs.regs[13]));
		hang_log("K64+ U32 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
			(long)(user_ret->user_regs.pc),
			(long)(user_ret->user_regs.regs[14]),
			(long)(user_ret->user_regs.regs[13]));
		log_hang_info("r12-r0 0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[12]),
			(long)(user_ret->user_regs.regs[11]),
			(long)(user_ret->user_regs.regs[10]),
			(long)(user_ret->user_regs.regs[9]));
		hang_log("r12-r0 0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[12]),
			(long)(user_ret->user_regs.regs[11]),
			(long)(user_ret->user_regs.regs[10]),
			(long)(user_ret->user_regs.regs[9]));
		log_hang_info("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[8]),
			(long)(user_ret->user_regs.regs[7]),
			(long)(user_ret->user_regs.regs[6]),
			(long)(user_ret->user_regs.regs[5]),
			(long)(user_ret->user_regs.regs[4]));
		hang_log("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[8]),
			(long)(user_ret->user_regs.regs[7]),
			(long)(user_ret->user_regs.regs[6]),
			(long)(user_ret->user_regs.regs[5]),
			(long)(user_ret->user_regs.regs[4]));
		log_hang_info("0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[3]),
			(long)(user_ret->user_regs.regs[2]),
			(long)(user_ret->user_regs.regs[1]),
			(long)(user_ret->user_regs.regs[0]));
		hang_log("0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[3]),
			(long)(user_ret->user_regs.regs[2]),
			(long)(user_ret->user_regs.regs[1]),
			(long)(user_ret->user_regs.regs[0]));
		userstack_start = (unsigned long)user_ret->user_regs.regs[13];
		vma = current_task->mm->mmap;
		while (vma) {
			if (vma->vm_start <= userstack_start &&
				vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}

		if (!userstack_end) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		length = userstack_end - userstack_start;

		/*  dump native stack to buffer */
		{
			unsigned long SPStart = 0, SPEnd = 0;
			int tempSpContent[4], copied;

			SPStart = userstack_start;
			SPEnd = SPStart + length;
			log_hang_info("UserSP_start:%lx,Length:%lx,End:%lx\n",
				SPStart, length, SPEnd);
			hang_log("UserSP_start:%lx,Length:%lx,End:%lx\n",
				SPStart, length, SPEnd);
			while (SPStart < SPEnd) {
				copied = access_process_vm(current_task,
						SPStart, &tempSpContent,
						sizeof(tempSpContent), 0);
				if (copied != sizeof(tempSpContent)) {
					pr_info(
					  "access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n",
					  (unsigned int)sizeof(tempSpContent));
					/* return -EIO; */
				}
				if (tempSpContent[0] != 0 ||
					tempSpContent[1] != 0 ||
					tempSpContent[2] != 0 ||
					tempSpContent[3] != 0) {
					log_hang_info("%08lx:%x %x %x %x\n",
							SPStart,
							tempSpContent[0],
							tempSpContent[1],
							tempSpContent[2],
							tempSpContent[3]);
					hang_log("%08lx:%x %x %x %x\n",
							SPStart,
							tempSpContent[0],
							tempSpContent[1],
							tempSpContent[2],
							tempSpContent[3]);
				}
				SPStart += 4 * 4;
			}
		}
	} else {		/*K64+U64 */
		userstack_start = (unsigned long)user_ret->user_regs.sp;

		vma = current_task->mm->mmap;
		while (vma) {
			if (vma->vm_start <= userstack_start &&
					vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		if (!userstack_end) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		{
			unsigned long tmpfp, tmp, tmpLR;
			int copied, frames;
			unsigned long native_bt[16];

			native_bt[0] = user_ret->user_regs.pc;
			native_bt[1] = user_ret->user_regs.regs[30];
			tmpfp = user_ret->user_regs.regs[29];
			frames = 2;
			while (tmpfp < userstack_end &&
					tmpfp > userstack_start) {
				copied =
				    access_process_vm(current_task,
						    (unsigned long)tmpfp, &tmp,
						      sizeof(tmp), 0);
				if (copied != sizeof(tmp)) {
					pr_info("access_process_vm  fp error\n");
					return -EIO;
				}
				copied =
				    access_process_vm(current_task,
						    (unsigned long)tmpfp + 0x08,
						      &tmpLR, sizeof(tmpLR), 0);
				if (copied != sizeof(tmpLR)) {
					pr_info("access_process_vm  pc error\n");
					return -EIO;
				}
				tmpfp = tmp;
				native_bt[frames] = tmpLR;
				frames++;
				if (frames >= 16)
					break;
			}
			for (copied = 0; copied < frames; copied++) {
				/*  #00 pc 000000000006c760
				 *  /system/lib64/ libc.so (__epoll_pwait+8)
				 */
				log_hang_info("#%d pc %lx\n", copied,
						native_bt[copied]);
				hang_log("#%d pc %lx\n", copied,
						native_bt[copied]);
			}
		}
	}
#endif

	return 0;
}

static void show_bt_by_pid(int task_pid)
{
	struct task_struct *t, *p;
	struct pid *pid;
#ifdef __aarch64__
	struct pt_regs *user_ret;
#endif
	int count = 0, dump_native = 0;
	unsigned int state = 0;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	pid = find_get_pid(task_pid);
	t = p = get_pid_task(pid, PIDTYPE_PID);

	if (p != NULL) {
		if (try_get_task_stack(p)) {
			log_hang_info("%s: %d: %s.\n", __func__, task_pid, t->comm);
			hang_log("%s: %d: %s.\n", __func__, task_pid, t->comm);
#ifndef __aarch64__	 /* 32bit */
			if (!strcmp(t->comm, "system_server"))
				dump_native = 1;
			else
				dump_native = 0;
#else
			user_ret = task_pt_regs(t);

			if (!user_mode(user_ret)) {
				pr_info(" %s,%d:%s,fail in user_mode", __func__,
						task_pid, t->comm);
				dump_native = 0;
			} else	if (!t->mm) {
				pr_info(" %s,%d:%s, current_task->mm == NULL", __func__,
						task_pid, t->comm);
				dump_native = 0;
			} else if (compat_user_mode(user_ret)) {
				/* K64_U32 for check reg */
				if (!strcmp(t->comm, "system_server"))
					dump_native = 1;
				else
					dump_native = 0;
			} else
				dump_native = 1;
#endif
			if (dump_native == 1)
				/* catch maps to Userthread_maps */
				dump_native_maps(task_pid, p);
			put_task_struct(p);
		} else {
			state = p->state ? __ffs(p->state) + 1 : 0;
			log_hang_info("%s pid %d state %c, flags %d. stack is null.\n",
				t->comm, task_pid, state < sizeof(stat_nam) - 1 ?
				stat_nam[state] : '?', t->flags);
			hang_log("%s pid %d state %c, flags %d. stack is null.\n",
				t->comm, task_pid, state < sizeof(stat_nam) - 1 ?
				stat_nam[state] : '?', t->flags);
		}
		do {
			if (t && try_get_task_stack(t)) {
				pid_t tid = 0;

				get_task_struct(t);
				tid = task_pid_vnr(t);
				state = t->state ? __ffs(t->state) + 1 : 0;
				/* catch kernel bt */
				show_thread_info(t, true);

				log_hang_info("%s sysTid=%d, pid=%d\n", t->comm,
						tid, task_pid);
				hang_log("%s sysTid=%d, pid=%d\n", t->comm,
						tid, task_pid);

				if (dump_native == 1)
					dump_native_info_by_tid(tid, t);
				put_task_struct(t);
			}
			if ((++count) % 5 == 4)
				msleep(20);
			log_hang_info("-\n");
		} while_each_thread(p, t);
	}
	put_pid(pid);
}

static int show_white_list_bt(struct task_struct *p)
{
	struct name_list *pList = NULL;

	if (!white_list)
		return -1;


	raw_spin_lock(&white_list_lock);
	pList = white_list;
	while (pList) {
		if (!strcmp(p->comm, pList->name)) {
			raw_spin_unlock(&white_list_lock);
			show_bt_by_pid(p->pid);
			return 0;
		}
		pList = pList->next;
	}
	raw_spin_unlock(&white_list_lock);
	return -1;
}

static void show_task_backtrace(void)
{
	struct task_struct *p, *t, *system_server_task = NULL;
	struct task_struct *monkey_task = NULL;
	struct task_struct *aee_aed_task = NULL;

#ifdef CONFIG_MTK_HANG_DETECT_DB
	watchdog_thread_exist = false;
	system_server_exist = false;
#endif
	log_hang_info("dump backtrace start: %llu\n", local_clock());

	rcu_read_lock();
	for_each_process(p) {
		get_task_struct(p);
		if (Hang_Detect_first == false) {
			if (!strcmp(p->comm, "system_server"))
				system_server_task = p;
			if (strstr(p->comm, "monkey"))
				monkey_task = p;
			if (!strcmp(p->comm, "aee_aed"))
				aee_aed_task = p;
		}
		/* specify process, need dump maps file and native backtrace */
		if (!strcmp(p->comm, "surfaceflinger") ||
			!strcmp(p->comm, "init") ||
			!strcmp(p->comm, "system_server") ||
			!strcmp(p->comm, "mmcqd/0")  ||
			!strcmp(p->comm, "debuggerd64") ||
			!strcmp(p->comm, "mmcqd/1") ||
			!strcmp(p->comm, "vold") ||
			!strcmp(p->comm, "vdc") ||
			!strcmp(p->comm, "debuggerd")) {
			show_bt_by_pid(p->pid);
			put_task_struct(p);
			continue;
		}
		if (!show_white_list_bt(p)) {
			put_task_struct(p);
			continue;
		}
		for_each_thread(p, t) {
			if (try_get_task_stack(t)) {
				get_task_struct(t);
				show_thread_info(t, false);
				put_task_struct(t);
			}
		}
		put_task_struct(p);
	}
	rcu_read_unlock();
	log_hang_info("dump backtrace end: %llu\n", local_clock());
	if (Hang_Detect_first == false) {
		if (aee_aed_task)
			send_sig_info(SIGUSR1, SEND_SIG_PRIV,
				aee_aed_task);
		if (system_server_task)
			send_sig_info(SIGQUIT, SEND_SIG_PRIV,
				system_server_task);
		if (monkey_task)
			send_sig_info(SIGQUIT, SEND_SIG_PRIV,
				monkey_task);
	}
}

static void show_status(int flag)
{

#ifdef CONFIG_MTK_HANG_DETECT_DB
#ifndef MODULE
	if (Hang_Detect_first)	{ /* the last dump */
		dump_mem_info();
		dump_msdc_hang_info();
	}
#endif
#endif

	show_task_backtrace();

	if (Hang_Detect_first)	{ /* the last dump */
		/* debug_locks = 1; */
		debug_show_all_locks();
#ifndef MODULE
		show_free_areas(0, NULL);
		if (show_task_mem)
			show_task_mem();
#endif

#if IS_ENABLED(CONFIG_MTK_WQ_DEBUG)
		wq_debug_dump();
#endif

	}
}

static int dump_last_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};

	sched_setscheduler(current, SCHED_FIFO, &param);
	pr_info("[Hang_Detect] dump last thread.\n");
	show_status(1);
	dump_bt_done = 1;
	wake_up_interruptible(&dump_bt_done_wait);
	return 0;
}

static int hang_detect_dump_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};

	sched_setscheduler(current, SCHED_FIFO, &param);
	msleep(120 * 1000);
	dump_bt_done = 1;
	while (1) {
		wait_event_interruptible(dump_bt_start_wait, dump_bt_done == 0);
		show_status(0);
		dump_bt_done = 1;
		wake_up_interruptible(&dump_bt_done_wait);
	}
	pr_notice("[Hang_Detect] hang_detect dump thread exit.\n");
	return 0;
}

void wake_up_dump(void)
{
	dump_bt_done = 0;
	wake_up_interruptible(&dump_bt_start_wait);
	if (dump_bt_done != 1)
		wait_event_interruptible_timeout(dump_bt_done_wait,
			dump_bt_done == 1, HZ*10);
}


bool check_white_list(void)
{
	struct name_list *pList = NULL;

	if (!white_list)
		return true;
	raw_spin_lock(&white_list_lock);
	pList = white_list;
	while (pList) {
		if (find_task_by_name(pList->name) < 0) {
			/* not fond the Task */
			raw_spin_unlock(&white_list_lock);
			return false;
		}
		pList = pList->next;
	}
	raw_spin_unlock(&white_list_lock);
	return true;
}

static int hang_detect_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};
	struct task_struct *hd_thread;

	sched_setscheduler(current, SCHED_FIFO, &param);
	reset_hang_info();
	msleep(120 * 1000);
	pr_debug("[Hang_Detect] hang_detect thread starts.\n");

#ifdef BOOT_UP_HANG
	hd_timeout = 9;
	hang_detect_counter = 9;
	hd_detect_enabled = true;
#endif

	while (1) {
		pr_info("[Hang_Detect] hang_detect thread counts down %d:%d, status %d.\n",
			hang_detect_counter, hd_timeout, hd_detect_enabled);
#ifdef BOOT_UP_HANG
		if (hd_detect_enabled)
#else
		if (hd_detect_enabled && check_white_list())
#endif
		{

			if (hang_detect_counter <= 0) {
				log_hang_info(
					"[Hang_detect]Dump the %d time process bt.\n",
					Hang_Detect_first ? 2 : 1);
#ifdef CONFIG_MTK_HANG_DETECT_DB
				if (!Hang_Detect_first) {
					memset(Hang_Info, 0, MaxHangInfoSize);
					Hang_Info_Size = 0;
				}
#endif
				if (Hang_Detect_first == true
					&& dump_bt_done != 1) {
		/* some time dump thread will block in dumping native bt */
		/* so create new thread to dump enough kernel bt */
					hd_thread = kthread_create(
						dump_last_thread,
						NULL, "hang_detect2");
					if (hd_thread)
						wake_up_process(hd_thread);
				if (dump_bt_done != 1)
					wait_event_interruptible_timeout(
							dump_bt_done_wait,
							dump_bt_done == 1,
							HZ*10);
				} else
					wake_up_dump();


				if (Hang_Detect_first == true) {
#ifdef CONFIG_MTK_HANG_DETECT_DB
					trigger_hang_db();
#else
					BUG();
#endif
				} else
					Hang_Detect_first = true;
			}
			hang_detect_counter--;
		}

		msleep((HD_INTER) * 1000);
	}
	return 0;
}

void monitor_hang_kick(int lParam)
{
	if (reboot_flag) {
		pr_info("[Hang_Detect] in reboot flow.\n");
		return;
	}

	if (lParam == 0) {
		hd_detect_enabled = 0;
		hang_detect_counter = hd_timeout;
		pr_info("[Hang_Detect] hang_detect disabled\n");
	} else if (lParam > 0) {
		/* lParem=0x1000|timeout,only set in aee call when NE in
		 *  system_server so only change hang_detect_counter when
		 *  call from AEE
		 * Others ioctl, will change
		 * hd_detect_enabled & hang_detect_counter
		 */
		if (lParam & 0x1000) {
			hang_detect_counter = hd_timeout =
			  ((long)(lParam & 0x0fff) + HD_INTER - 1) / (HD_INTER);
		} else {
			hd_detect_enabled = 1;
			hang_detect_counter = hd_timeout =
			    ((long)lParam + HD_INTER - 1) / (HD_INTER);
		}

		if (hd_timeout < 3) {
			/* hang detect min timeout is 10 (5min) */
			hang_detect_counter = 3;
			hd_timeout = 3;
		}
		pr_info("[Hang_Detect] hang_detect enabled %d\n", hd_timeout);
	}
	reset_hang_info();
}

int hang_detect_init(void)
{

	struct task_struct *hd_thread;

	pr_debug("[Hang_Detect] Initialize proc\n");
	hd_thread = kthread_create(hang_detect_thread, NULL, "hang_detect");
	if (hd_thread)
		wake_up_process(hd_thread);

	hd_thread = kthread_create(hang_detect_dump_thread, NULL,
			"hang_detect1");
	if (hd_thread)
		wake_up_process(hd_thread);

	return 0;
}

static int __init monitor_hang_init(void)
{
	int err = 0;

#ifdef CONFIG_MTK_HANG_DETECT_DB
	Hang_Info = kmalloc(MAX_HANG_INFO_SIZE, GFP_KERNEL);
	if (Hang_Info == NULL)
		return 1;
#endif

	err = misc_register(&Hang_Monitor_dev);
	if (unlikely(err)) {
		pr_notice("failed to register Hang_Monitor_dev device!\n");
		return err;
	}
	hang_detect_init();
	return err;
}

static void __exit monitor_hang_exit(void)
{
	misc_deregister(&Hang_Monitor_dev);
#ifdef CONFIG_MTK_HANG_DETECT_DB
	/* kfree(NULL) is safe */
	kfree(Hang_Info);
#endif
}

module_init(monitor_hang_init);
module_exit(monitor_hang_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MonitorHang Driver");
MODULE_AUTHOR("MediaTek Inc.");
