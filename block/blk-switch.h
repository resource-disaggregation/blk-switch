/*
 *	"Rearchitecting Linux Storage Stack for
 *		microsecond Latency and High Throughput",
 *		USENIX OSDI 2021.
 *
 *	Authors:
 *		Jaehyun Hwang <jaehyun.hwang@cornell.edu>
 *		Midhul Vuppalapati <mvv25@cornell.edu>
 *		Simon Peter <simon@cs.utexas.edu>
 *		Rachit Agarwal <ragarwal@cs.cornell.edu>
 *
 *	SPDX-License-Identifier: GPL-2.0
 */

#include <linux/random.h>
#include <linux/sched/task.h>

#define BLK_SWITCH_TCP_BATCH	16

static int blk_switch_on __read_mostly;
module_param(blk_switch_on, int, 0644);
MODULE_PARM_DESC(blk_switch_on, "0: prioritization, 1: +reqstr, 2: +appstr ");

static int blk_switch_interval_us __read_mostly = 10000;
module_param(blk_switch_interval_us, int, 0644);
MODULE_PARM_DESC(blk_switch_interval_us, "blk-switch interval (us) for app-steering ");

static int blk_switch_thresh_B __read_mostly = 16;
module_param(blk_switch_thresh_B, int, 0644);
MODULE_PARM_DESC(blk_switch_thresh_B, "blk-switch threshold for B");

static int blk_switch_thresh_L __read_mostly;
module_param(blk_switch_thresh_L, int, 0644);
MODULE_PARM_DESC(blk_switch_thresh_L, "blk-switch threshold for L*");

static int blk_switch_nr_cpus __read_mostly = 24;
module_param(blk_switch_nr_cpus, int, 0644);
MODULE_PARM_DESC(blk_switch_nr_cpus, "blk-switch nr cpus");

static int blk_switch_printk __read_mostly;
module_param(blk_switch_printk, int, 0644);
MODULE_PARM_DESC(blk_switch_printk, "blk-switch printk on/off");

unsigned long    blk_switch_thru_bytes[NR_CPUS] = {0};
unsigned long    blk_switch_lat_bytes[NR_CPUS] = {0};
unsigned long    blk_switch_metric_thru[NR_CPUS] = {0};		// T_(cur_cpu)
unsigned long    blk_switch_metric_lat[NR_CPUS] = {0};		// L_(cur_cpu)
unsigned long    blk_switch_reset_metrics = 0;

int blk_switch_lat_apps[NR_CPUS] = {1};

/* stats */
int blk_switch_thru_count[6] = {0, 0, 0, 0, 0, 0};
int blk_switch_thru_new4[6] = {0, 0, 0, 0, 0, 0};
int blk_switch_thru_steer[6] = {0, 0, 0, 0, 0, 0};

enum blk_switch_ioprio {
	BLK_SWITCH_T_APP = 0,
	BLK_SWITCH_L_APP,
}

enum blk_swtich_fabric {
	BLK_SWITCH_NONE = 0,
	BLK_SWITCH_TCP,
	BLK_SWITCH_RDMA,
};

struct nvme_tcp_queue {
	struct socket           *sock;
	struct work_struct      io_work;
	int                     io_cpu;

	struct work_struct      io_work_lat;
	int                     prio_class;

	spinlock_t              lock;
	struct list_head        send_list;

	/* recv state */
	void                    *pdu;
	int                     pdu_remaining;
	int                     pdu_offset;
	size_t                  data_remaining;
	size_t                  ddgst_remaining;
	unsigned int            nr_cqe;

	/* send state */
	struct nvme_tcp_request *request;

	int                     queue_size;
	size_t                  cmnd_capsule_len;
	struct nvme_tcp_ctrl    *ctrl;
	unsigned long           flags;
	bool                    rd_enabled;

	bool                    hdr_digest;
	bool                    data_digest;
	struct ahash_request    *rcv_hash;
	struct ahash_request    *snd_hash;
	__le32                  exp_ddgst;
	__le32                  recv_ddgst;

	/* jaehyun: For i10 caravans */
	struct kvec             *caravan_iovs;
	size_t                  caravan_len;
	int                     nr_iovs;
	bool                    send_now;
	int                     nr_caravan_req;

	/* jaehyun: For i10 delayed doorbells */
	atomic_t                nr_req;
	struct hrtimer          doorbell_timer;
	atomic_t                timer_set;

	struct page_frag_cache  pf_cache;

	void (*state_change)(struct sock *);
	void (*data_ready)(struct sock *);
	void (*write_space)(struct sock *);
};


static void blk_switch_set_ioprio(struct task_struct *p, struct bio *bio)
{
	int ioprio;

	ioprio = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_NONE, IOPRIO_NORM);
	task_lock(p);
	if (p->io_context)
		ioprio = p->io_context->ioprio;
	task_unlock(p);

	if (IOPRIO_PRIO_CLASS(ioprio) == BLK_SWITCH_L_APP) {
		bio_set_prio(bio, ioprio);
		bio->bi_opf |= REQ_PRIO;
	}
}

static inline bool blk_switch_request(struct bio *bio,
				struct blk_mq_alloc_data *data)
{
	return data->hctx->blk_switch && bio &&
		bio->bi_iter.bi_size > 0;
}

static inline bool blk_switch_is_thru_request(struct bio *bio)
{
	return bio && IOPRIO_PRIO_CLASS(bio_prio(bio)) != BLK_SWITCH_L_APP;
}
