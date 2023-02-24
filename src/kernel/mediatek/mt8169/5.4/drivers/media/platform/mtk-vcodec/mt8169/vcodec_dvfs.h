/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Cheng-Jung Ho <cheng-jung.ho@mediatek.com>
 */


#ifndef __VCODEC_DVFS_H__
#define __VCODEC_DVFS_H__

#define MAX_HISTORY 10
#define MIN_SUBMIT_GAP 2000		/* 2ms */
#define MAX_SUBMIT_GAP (1000*1000)	/* 1 second */
#define FREE_HIST_DELAY (5000*1000)	/* Free history delay */
#define DEFAULT_MHZ 546
#define DEC_MODULE 2

#ifdef CONFIG_ARM64
#define do_div_dvfs do_div
#else
#define do_div_dvfs(n, d) ({\
			s32 __rem; \
			(n) = div_s64_rem((n), (d), &__rem); \
			__rem; \
		})
#endif

struct codec_history {
	void *handle;
	int kcy[MAX_HISTORY];
	unsigned long submit[MAX_HISTORY];
	unsigned long start[MAX_HISTORY];
	unsigned long end[MAX_HISTORY];
	unsigned long sw_time[MAX_HISTORY];
	unsigned long submit_interval;
	int cur_idx;
	int cur_cnt;
	int tot_kcy;
	unsigned long tot_time;
	struct codec_history *next;
};

struct codec_job {
	void *handle;
	unsigned long submit;
	unsigned long start;
	unsigned long end;
	int hw_kcy;
	int mhz;
	struct codec_job *next;
};

struct codec_freq {
	unsigned long freq[DEC_MODULE];//MODULE yinbo temp
	unsigned long active_freq;
};

unsigned long get_time_us(void);

/* Add a new job to job queue */
struct codec_job *add_job(void *handle, struct codec_job **head);

/* Move target job to queue head for processing */
struct codec_job *move_job_to_head(void *handle, struct codec_job **head);

/* Update history with completed job */
int update_hist(struct codec_job *job, struct codec_history **head,
		unsigned long submit_interval);

/* Estimate required freq from job queue and previous history */
int est_freq(void *handle, struct codec_job **job, struct codec_history *head);
unsigned long match_freq(
	unsigned long target_hz, unsigned long *freq_list, u32 freq_cnt, unsigned long max_freq_hz);

/* Free unused/all history */
int free_hist(struct codec_history **head, int only_unused);
int free_hist_by_handle(void *handle, struct codec_history **head);
#endif
