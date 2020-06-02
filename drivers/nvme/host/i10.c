/*
 *	TCP~~RDMA: CPU-efficient Remote Storage Access
 *			with i10
 *		- i10 host implementation
 *		(inspired by drivers/nvme/host/tcp.c)
 *
 *	Authors:
 *		Jaehyun Hwang <jaehyun.hwang@cornell.edu>
 *		Qizhe Cai <qc228@cornell.edu>
 *		A. Kevin Tang <atang@cornell.edu>
 *		Rachit Agarwal <ragarwal@cs.cornell.edu>
 *
 *	SPDX-License-Identifier: GPL-2.0
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/nvme-tcp.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <linux/blk-mq.h>
#include <crypto/hash.h>
#include <net/busy_poll.h>

#include <linux/bio.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include "nvme.h"
#include "fabrics.h"

#define I10_CARAVAN_CAPACITY		65536
#define I10_AGGREGATION_SIZE		16
#define I10_MIN_DOORBELL_TIMEOUT	25

static int i10_delayed_doorbell_us __read_mostly = 50;
module_param(i10_delayed_doorbell_us, int, 0644);
MODULE_PARM_DESC(i10_delayed_doorbell_us,
		"i10 delayed doorbell timer (us)");

struct i10_host_queue;

enum i10_host_send_state {
	I10_HOST_SEND_CMD_PDU = 0,
	I10_HOST_SEND_H2C_PDU,
	I10_HOST_SEND_DATA,
	I10_HOST_SEND_DDGST,
};

struct i10_host_request {
	struct nvme_request	req;
	void			*pdu;
	struct i10_host_queue	*queue;
	u32			data_len;
	u32			pdu_len;
	u32			pdu_sent;
	u16			ttag;
	struct list_head	entry;
	u32			ddgst;

	struct bio		*curr_bio;
	struct iov_iter		iter;

	/* send state */
	size_t			offset;
	size_t			data_sent;
	enum i10_host_send_state state;
};

enum i10_host_queue_flags {
	NVME_TCP_Q_ALLOCATED	= 0,
	NVME_TCP_Q_LIVE		= 1,
};

enum i10_host_recv_state {
	NVME_TCP_RECV_PDU = 0,
	NVME_TCP_RECV_DATA,
	NVME_TCP_RECV_DDGST,
};

struct i10_host_ctrl;
struct i10_host_queue {
	struct socket		*sock;
	struct work_struct	io_work;
	int			io_cpu;

	spinlock_t		lock;
	struct list_head	send_list;

	/* recv state */
	void			*pdu;
	int			pdu_remaining;
	int			pdu_offset;
	size_t			data_remaining;
	size_t			ddgst_remaining;
	unsigned int		nr_cqe;

	/* send state */
	struct i10_host_request *request;

	int			queue_size;
	size_t			cmnd_capsule_len;
	struct i10_host_ctrl	*ctrl;
	unsigned long		flags;
	bool			rd_enabled;

	bool			hdr_digest;
	bool			data_digest;
	struct ahash_request	*rcv_hash;
	struct ahash_request	*snd_hash;
	__le32			exp_ddgst;
	__le32			recv_ddgst;

	/* For i10 caravans */
	struct kvec		*caravan_iovs;
	size_t			caravan_len;
	int			nr_iovs;
	bool			send_now;

	struct page		**caravan_mapped;
	int			nr_mapped;

	/* For i10 delayed doorbells */
	int			nr_req;
	bool			doorbell_expire;
	struct hrtimer		doorbell_timer;

	struct page_frag_cache	pf_cache;

	void (*state_change)(struct sock *);
	void (*data_ready)(struct sock *);
	void (*write_space)(struct sock *);
};

struct i10_host_ctrl {
	/* read only in the hot path */
	struct i10_host_queue	*queues;
	struct blk_mq_tag_set	tag_set;

	/* other member variables */
	struct list_head	list;
	struct blk_mq_tag_set	admin_tag_set;
	struct sockaddr_storage addr;
	struct sockaddr_storage src_addr;
	struct nvme_ctrl	ctrl;

	struct work_struct	err_work;
	struct delayed_work	connect_work;
	struct i10_host_request async_req;
	u32			io_queues[HCTX_MAX_TYPES];
};

static LIST_HEAD(i10_host_ctrl_list);
static DEFINE_MUTEX(i10_host_ctrl_mutex);
static struct workqueue_struct *i10_host_wq;
static struct blk_mq_ops i10_host_mq_ops;
static struct blk_mq_ops i10_host_admin_mq_ops;

static inline struct i10_host_ctrl *to_i10_host_ctrl(struct nvme_ctrl *ctrl)
{
	return container_of(ctrl, struct i10_host_ctrl, ctrl);
}

static inline int i10_host_queue_id(struct i10_host_queue *queue)
{
	return queue - queue->ctrl->queues;
}

static inline struct blk_mq_tags *i10_host_tagset(struct i10_host_queue *queue)
{
	u32 queue_idx = i10_host_queue_id(queue);

	if (queue_idx == 0)
		return queue->ctrl->admin_tag_set.tags[queue_idx];
	return queue->ctrl->tag_set.tags[queue_idx - 1];
}

static inline u8 i10_host_hdgst_len(struct i10_host_queue *queue)
{
	return queue->hdr_digest ? NVME_TCP_DIGEST_LENGTH : 0;
}

static inline u8 i10_host_ddgst_len(struct i10_host_queue *queue)
{
	return queue->data_digest ? NVME_TCP_DIGEST_LENGTH : 0;
}

static inline size_t i10_host_inline_data_size(struct i10_host_queue *queue)
{
	return queue->cmnd_capsule_len - sizeof(struct nvme_command);
}

static inline bool i10_host_async_req(struct i10_host_request *req)
{
	return req == &req->queue->ctrl->async_req;
}

static inline bool i10_host_has_inline_data(struct i10_host_request *req)
{
	struct request *rq;

	if (unlikely(i10_host_async_req(req)))
		return false; /* async events don't have a request */

	rq = blk_mq_rq_from_pdu(req);

	return rq_data_dir(rq) == WRITE && req->data_len &&
		req->data_len <= i10_host_inline_data_size(req->queue);
}

static inline struct page *i10_host_req_cur_page(struct i10_host_request *req)
{
	return req->iter.bvec->bv_page;
}

static inline size_t i10_host_req_cur_offset(struct i10_host_request *req)
{
	return req->iter.bvec->bv_offset + req->iter.iov_offset;
}

static inline size_t i10_host_req_cur_length(struct i10_host_request *req)
{
	return min_t(size_t, req->iter.bvec->bv_len - req->iter.iov_offset,
			req->pdu_len - req->pdu_sent);
}

static inline size_t i10_host_req_offset(struct i10_host_request *req)
{
	return req->iter.iov_offset;
}

static inline size_t i10_host_pdu_data_left(struct i10_host_request *req)
{
	return rq_data_dir(blk_mq_rq_from_pdu(req)) == WRITE ?
			req->pdu_len - req->pdu_sent : 0;
}

static inline size_t i10_host_pdu_last_send(struct i10_host_request *req,
		int len)
{
	return i10_host_pdu_data_left(req) <= len;
}

static void i10_host_init_iter(struct i10_host_request *req,
		unsigned int dir)
{
	struct request *rq = blk_mq_rq_from_pdu(req);
	struct bio_vec *vec;
	unsigned int size;
	int nsegs;
	size_t offset;

	if (rq->rq_flags & RQF_SPECIAL_PAYLOAD) {
		vec = &rq->special_vec;
		nsegs = 1;
		size = blk_rq_payload_bytes(rq);
		offset = 0;
	} else {
		struct bio *bio = req->curr_bio;

		vec = __bvec_iter_bvec(bio->bi_io_vec, bio->bi_iter);
		nsegs = bio_segments(bio);
		size = bio->bi_iter.bi_size;
		offset = bio->bi_iter.bi_bvec_done;
	}

	iov_iter_bvec(&req->iter, dir, vec, nsegs, size);
	req->iter.iov_offset = offset;
}

static inline void i10_host_advance_req(struct i10_host_request *req,
		int len)
{
	req->data_sent += len;
	req->pdu_sent += len;
	iov_iter_advance(&req->iter, len);
	if (!iov_iter_count(&req->iter) &&
	    req->data_sent < req->data_len) {
		req->curr_bio = req->curr_bio->bi_next;
		i10_host_init_iter(req, WRITE);
	}
}

static inline bool i10_host_legacy_path(struct i10_host_request *req)
{
	return (i10_host_queue_id(req->queue) == 0) ||
		(i10_delayed_doorbell_us < I10_MIN_DOORBELL_TIMEOUT);
}

#define I10_ALLOWED_FLAGS (REQ_OP_READ | REQ_OP_WRITE | REQ_DRV | \
		REQ_RAHEAD | REQ_SYNC | REQ_IDLE | REQ_NOMERGE)

static bool i10_host_is_nodelay_path(struct i10_host_request *req)
{
	return (req->curr_bio == NULL) ||
		(req->curr_bio->bi_opf & ~I10_ALLOWED_FLAGS);
}

static inline void i10_host_queue_request(struct i10_host_request *req)
{
	struct i10_host_queue *queue = req->queue;

	spin_lock(&queue->lock);
	list_add_tail(&req->entry, &queue->send_list);
	spin_unlock(&queue->lock);

	if (!i10_host_legacy_path(req) && !i10_host_is_nodelay_path(req)) {
		queue->nr_req++;

		/* Start a delayed doorbell timer */
		if (queue->doorbell_expire) {
			hrtimer_start(&queue->doorbell_timer,
				ns_to_ktime(i10_delayed_doorbell_us *
					NSEC_PER_USEC),
				HRTIMER_MODE_REL);
			queue->doorbell_expire = false;
		}
		/* Ring the delayed doorbell
		 * if I/O request counter >= i10 aggregation size
		 */
		else if (queue->nr_req >= I10_AGGREGATION_SIZE) {
			hrtimer_cancel(&queue->doorbell_timer);
			queue->doorbell_expire = true;
			queue->nr_req = 0;
			queue_work_on(queue->io_cpu, i10_host_wq,
					&queue->io_work);
		}
	}
	/* Ring the doorbell immediately for no-delay path */
	else {
		if (!queue->doorbell_expire) {
			hrtimer_cancel(&queue->doorbell_timer);
			queue->doorbell_expire = true;
			queue->nr_req = 0;
		}
		queue_work_on(queue->io_cpu, i10_host_wq, &queue->io_work);
	}
}

static inline struct i10_host_request *
i10_host_fetch_request(struct i10_host_queue *queue)
{
	struct i10_host_request *req;

	spin_lock(&queue->lock);
	req = list_first_entry_or_null(&queue->send_list,
			struct i10_host_request, entry);
	if (req)
		list_del(&req->entry);
	spin_unlock(&queue->lock);

	return req;
}

static inline void i10_host_ddgst_final(struct ahash_request *hash,
		__le32 *dgst)
{
	ahash_request_set_crypt(hash, NULL, (u8 *)dgst, 0);
	crypto_ahash_final(hash);
}

static inline void i10_host_ddgst_update(struct ahash_request *hash,
		struct page *page, off_t off, size_t len)
{
	struct scatterlist sg;

	sg_init_marker(&sg, 1);
	sg_set_page(&sg, page, len, off);
	ahash_request_set_crypt(hash, &sg, NULL, len);
	crypto_ahash_update(hash);
}

static inline void i10_host_hdgst(struct ahash_request *hash,
		void *pdu, size_t len)
{
	struct scatterlist sg;

	sg_init_one(&sg, pdu, len);
	ahash_request_set_crypt(hash, &sg, pdu + len, len);
	crypto_ahash_digest(hash);
}

static int i10_host_verify_hdgst(struct i10_host_queue *queue,
		void *pdu, size_t pdu_len)
{
	struct nvme_tcp_hdr *hdr = pdu;
	__le32 recv_digest;
	__le32 exp_digest;

	if (unlikely(!(hdr->flags & NVME_TCP_F_HDGST))) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d: header digest flag is cleared\n",
			i10_host_queue_id(queue));
		return -EPROTO;
	}

	recv_digest = *(__le32 *)(pdu + hdr->hlen);
	i10_host_hdgst(queue->rcv_hash, pdu, pdu_len);
	exp_digest = *(__le32 *)(pdu + hdr->hlen);
	if (recv_digest != exp_digest) {
		dev_err(queue->ctrl->ctrl.device,
			"header digest error: recv %#x expected %#x\n",
			le32_to_cpu(recv_digest), le32_to_cpu(exp_digest));
		return -EIO;
	}

	return 0;
}

static int i10_host_check_ddgst(struct i10_host_queue *queue, void *pdu)
{
	struct nvme_tcp_hdr *hdr = pdu;
	u8 digest_len = i10_host_hdgst_len(queue);
	u32 len;

	len = le32_to_cpu(hdr->plen) - hdr->hlen -
		((hdr->flags & NVME_TCP_F_HDGST) ? digest_len : 0);

	if (unlikely(len && !(hdr->flags & NVME_TCP_F_DDGST))) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d: data digest flag is cleared\n",
		i10_host_queue_id(queue));
		return -EPROTO;
	}
	crypto_ahash_init(queue->rcv_hash);

	return 0;
}

static void i10_host_exit_request(struct blk_mq_tag_set *set,
		struct request *rq, unsigned int hctx_idx)
{
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);

	page_frag_free(req->pdu);
}

static int i10_host_init_request(struct blk_mq_tag_set *set,
		struct request *rq, unsigned int hctx_idx,
		unsigned int numa_node)
{
	struct i10_host_ctrl *ctrl = set->driver_data;
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);
	int queue_idx = (set == &ctrl->tag_set) ? hctx_idx + 1 : 0;
	struct i10_host_queue *queue = &ctrl->queues[queue_idx];
	u8 hdgst = i10_host_hdgst_len(queue);

	req->pdu = page_frag_alloc(&queue->pf_cache,
		sizeof(struct nvme_tcp_cmd_pdu) + hdgst,
		GFP_KERNEL | __GFP_ZERO);
	if (!req->pdu)
		return -ENOMEM;

	req->queue = queue;
	nvme_req(rq)->ctrl = &ctrl->ctrl;

	return 0;
}

static int i10_host_init_hctx(struct blk_mq_hw_ctx *hctx, void *data,
		unsigned int hctx_idx)
{
	struct i10_host_ctrl *ctrl = data;
	struct i10_host_queue *queue = &ctrl->queues[hctx_idx + 1];

	hctx->driver_data = queue;
	return 0;
}

static int i10_host_init_admin_hctx(struct blk_mq_hw_ctx *hctx, void *data,
		unsigned int hctx_idx)
{
	struct i10_host_ctrl *ctrl = data;
	struct i10_host_queue *queue = &ctrl->queues[0];

	hctx->driver_data = queue;
	return 0;
}

static enum i10_host_recv_state
i10_host_recv_state(struct i10_host_queue *queue)
{
	return  (queue->pdu_remaining) ? NVME_TCP_RECV_PDU :
		(queue->ddgst_remaining) ? NVME_TCP_RECV_DDGST :
		NVME_TCP_RECV_DATA;
}

static void i10_host_init_recv_ctx(struct i10_host_queue *queue)
{
	queue->pdu_remaining = sizeof(struct nvme_tcp_rsp_pdu) +
				i10_host_hdgst_len(queue);
	queue->pdu_offset = 0;
	queue->data_remaining = -1;
	queue->ddgst_remaining = 0;
}

static void i10_host_error_recovery(struct nvme_ctrl *ctrl)
{
	if (!nvme_change_ctrl_state(ctrl, NVME_CTRL_RESETTING))
		return;

	queue_work(nvme_wq, &to_i10_host_ctrl(ctrl)->err_work);
}

static int i10_host_process_nvme_cqe(struct i10_host_queue *queue,
		struct nvme_completion *cqe)
{
	struct request *rq;

	rq = blk_mq_tag_to_rq(i10_host_tagset(queue), cqe->command_id);
	if (!rq) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d tag 0x%x not found\n",
			i10_host_queue_id(queue), cqe->command_id);
		i10_host_error_recovery(&queue->ctrl->ctrl);
		return -EINVAL;
	}

	nvme_end_request(rq, cqe->status, cqe->result);
	queue->nr_cqe++;

	return 0;
}

static int i10_host_handle_c2h_data(struct i10_host_queue *queue,
		struct nvme_tcp_data_pdu *pdu)
{
	struct request *rq;

	rq = blk_mq_tag_to_rq(i10_host_tagset(queue), pdu->command_id);
	if (!rq) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d tag %#x not found\n",
			i10_host_queue_id(queue), pdu->command_id);
		return -ENOENT;
	}

	if (!blk_rq_payload_bytes(rq)) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d tag %#x unexpected data\n",
			i10_host_queue_id(queue), rq->tag);
		return -EIO;
	}

	queue->data_remaining = le32_to_cpu(pdu->data_length);

	if (pdu->hdr.flags & NVME_TCP_F_DATA_SUCCESS &&
		unlikely(!(pdu->hdr.flags & NVME_TCP_F_DATA_LAST))) {
			dev_err(queue->ctrl->ctrl.device,
				"queue %d tag %#x SUCCESS set but not last PDU\n",
				i10_host_queue_id(queue), rq->tag);
			i10_host_error_recovery(&queue->ctrl->ctrl);
			return -EPROTO;
	}

	return 0;

}

static int i10_host_handle_comp(struct i10_host_queue *queue,
		struct nvme_tcp_rsp_pdu *pdu)
{
	struct nvme_completion *cqe = &pdu->cqe;
	int ret = 0;

	/*
	 * AEN requests are special as they don't time out and can
	 * survive any kind of queue freeze and often don't respond to
	 * aborts.  We don't even bother to allocate a struct request
	 * for them but rather special case them here.
	 */
	if (unlikely(i10_host_queue_id(queue) == 0 &&
	    cqe->command_id >= NVME_AQ_BLK_MQ_DEPTH))
		nvme_complete_async_event(&queue->ctrl->ctrl, cqe->status,
				&cqe->result);
	else
		ret = i10_host_process_nvme_cqe(queue, cqe);

	return ret;
}

static int i10_host_setup_h2c_data_pdu(struct i10_host_request *req,
		struct nvme_tcp_r2t_pdu *pdu)
{
	struct nvme_tcp_data_pdu *data = req->pdu;
	struct i10_host_queue *queue = req->queue;
	struct request *rq = blk_mq_rq_from_pdu(req);
	u8 hdgst = i10_host_hdgst_len(queue);
	u8 ddgst = i10_host_ddgst_len(queue);

	req->pdu_len = le32_to_cpu(pdu->r2t_length);
	req->pdu_sent = 0;

	if (unlikely(req->data_sent + req->pdu_len > req->data_len)) {
		dev_err(queue->ctrl->ctrl.device,
			"req %d r2t len %u exceeded data len %u (%zu sent)\n",
			rq->tag, req->pdu_len, req->data_len,
			req->data_sent);
		return -EPROTO;
	}

	if (unlikely(le32_to_cpu(pdu->r2t_offset) < req->data_sent)) {
		dev_err(queue->ctrl->ctrl.device,
			"req %d unexpected r2t offset %u (expected %zu)\n",
			rq->tag, le32_to_cpu(pdu->r2t_offset),
			req->data_sent);
		return -EPROTO;
	}

	memset(data, 0, sizeof(*data));
	data->hdr.type = nvme_tcp_h2c_data;
	data->hdr.flags = NVME_TCP_F_DATA_LAST;
	if (queue->hdr_digest)
		data->hdr.flags |= NVME_TCP_F_HDGST;
	if (queue->data_digest)
		data->hdr.flags |= NVME_TCP_F_DDGST;
	data->hdr.hlen = sizeof(*data);
	data->hdr.pdo = data->hdr.hlen + hdgst;
	data->hdr.plen =
		cpu_to_le32(data->hdr.hlen + hdgst + req->pdu_len + ddgst);
	data->ttag = pdu->ttag;
	data->command_id = rq->tag;
	data->data_offset = cpu_to_le32(req->data_sent);
	data->data_length = cpu_to_le32(req->pdu_len);
	return 0;
}

static int i10_host_handle_r2t(struct i10_host_queue *queue,
		struct nvme_tcp_r2t_pdu *pdu)
{
	struct i10_host_request *req;
	struct request *rq;
	int ret;

	rq = blk_mq_tag_to_rq(i10_host_tagset(queue), pdu->command_id);
	if (!rq) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d tag %#x not found\n",
			i10_host_queue_id(queue), pdu->command_id);
		return -ENOENT;
	}
	req = blk_mq_rq_to_pdu(rq);

	ret = i10_host_setup_h2c_data_pdu(req, pdu);
	if (unlikely(ret))
		return ret;

	req->state = I10_HOST_SEND_H2C_PDU;
	req->offset = 0;

	i10_host_queue_request(req);

	return 0;
}

static int i10_host_recv_pdu(struct i10_host_queue *queue, struct sk_buff *skb,
		unsigned int *offset, size_t *len)
{
	struct nvme_tcp_hdr *hdr;
	char *pdu = queue->pdu;
	size_t rcv_len = min_t(size_t, *len, queue->pdu_remaining);
	int ret;

	ret = skb_copy_bits(skb, *offset,
		&pdu[queue->pdu_offset], rcv_len);
	if (unlikely(ret))
		return ret;

	queue->pdu_remaining -= rcv_len;
	queue->pdu_offset += rcv_len;
	*offset += rcv_len;
	*len -= rcv_len;
	if (queue->pdu_remaining)
		return 0;

	hdr = queue->pdu;
	if (queue->hdr_digest) {
		ret = i10_host_verify_hdgst(queue, queue->pdu, hdr->hlen);
		if (unlikely(ret))
			return ret;
	}


	if (queue->data_digest) {
		ret = i10_host_check_ddgst(queue, queue->pdu);
		if (unlikely(ret))
			return ret;
	}

	switch (hdr->type) {
	case nvme_tcp_c2h_data:
		return i10_host_handle_c2h_data(queue, (void *)queue->pdu);
	case nvme_tcp_rsp:
		i10_host_init_recv_ctx(queue);
		return i10_host_handle_comp(queue, (void *)queue->pdu);
	case nvme_tcp_r2t:
		i10_host_init_recv_ctx(queue);
		return i10_host_handle_r2t(queue, (void *)queue->pdu);
	default:
		dev_err(queue->ctrl->ctrl.device,
			"unsupported pdu type (%d)\n", hdr->type);
		return -EINVAL;
	}
}

static inline void i10_host_end_request(struct request *rq, u16 status)
{
	union nvme_result res = {};

	nvme_end_request(rq, cpu_to_le16(status << 1), res);
}

static int i10_host_recv_data(struct i10_host_queue *queue, struct sk_buff *skb,
			      unsigned int *offset, size_t *len)
{
	struct nvme_tcp_data_pdu *pdu = (void *)queue->pdu;
	struct i10_host_request *req;
	struct request *rq;

	rq = blk_mq_tag_to_rq(i10_host_tagset(queue), pdu->command_id);
	if (!rq) {
		dev_err(queue->ctrl->ctrl.device,
			"queue %d tag %#x not found\n",
			i10_host_queue_id(queue), pdu->command_id);
		return -ENOENT;
	}
	req = blk_mq_rq_to_pdu(rq);

	while (true) {
		int recv_len, ret;

		recv_len = min_t(size_t, *len, queue->data_remaining);
		if (!recv_len)
			break;

		if (!iov_iter_count(&req->iter)) {
			req->curr_bio = req->curr_bio->bi_next;

			/*
			 * If we don`t have any bios it means that controller
			 * sent more data than we requested, hence error
			 */
			if (!req->curr_bio) {
				dev_err(queue->ctrl->ctrl.device,
					"queue %d no space in request %#x",
					i10_host_queue_id(queue), rq->tag);
				i10_host_init_recv_ctx(queue);
				return -EIO;
			}
			i10_host_init_iter(req, READ);
		}

		/* we can read only from what is left in this bio */
		recv_len = min_t(size_t, recv_len,
				iov_iter_count(&req->iter));

		if (queue->data_digest)
			ret = skb_copy_and_hash_datagram_iter(skb, *offset,
				&req->iter, recv_len, queue->rcv_hash);
		else
			ret = skb_copy_datagram_iter(skb, *offset,
				&req->iter, recv_len);
		if (ret) {
			dev_err(queue->ctrl->ctrl.device,
				"queue %d failed to copy request %#x data",
				i10_host_queue_id(queue), rq->tag);
			return ret;
		}

		*len -= recv_len;
		*offset += recv_len;
		queue->data_remaining -= recv_len;
	}

	if (!queue->data_remaining) {
		if (queue->data_digest) {
			i10_host_ddgst_final(queue->rcv_hash, &queue->exp_ddgst);
			queue->ddgst_remaining = NVME_TCP_DIGEST_LENGTH;
		} else {
			if (pdu->hdr.flags & NVME_TCP_F_DATA_SUCCESS) {
				i10_host_end_request(rq, NVME_SC_SUCCESS);
				queue->nr_cqe++;
			}
			i10_host_init_recv_ctx(queue);
		}
	}

	return 0;
}

static int i10_host_recv_ddgst(struct i10_host_queue *queue,
		struct sk_buff *skb, unsigned int *offset, size_t *len)
{
	struct nvme_tcp_data_pdu *pdu = (void *)queue->pdu;
	char *ddgst = (char *)&queue->recv_ddgst;
	size_t recv_len = min_t(size_t, *len, queue->ddgst_remaining);
	off_t off = NVME_TCP_DIGEST_LENGTH - queue->ddgst_remaining;
	int ret;

	ret = skb_copy_bits(skb, *offset, &ddgst[off], recv_len);
	if (unlikely(ret))
		return ret;

	queue->ddgst_remaining -= recv_len;
	*offset += recv_len;
	*len -= recv_len;
	if (queue->ddgst_remaining)
		return 0;

	if (queue->recv_ddgst != queue->exp_ddgst) {
		dev_err(queue->ctrl->ctrl.device,
			"data digest error: recv %#x expected %#x\n",
			le32_to_cpu(queue->recv_ddgst),
			le32_to_cpu(queue->exp_ddgst));
		return -EIO;
	}

	if (pdu->hdr.flags & NVME_TCP_F_DATA_SUCCESS) {
		struct request *rq = blk_mq_tag_to_rq(i10_host_tagset(queue),
						pdu->command_id);

		i10_host_end_request(rq, NVME_SC_SUCCESS);
		queue->nr_cqe++;
	}

	i10_host_init_recv_ctx(queue);
	return 0;
}

static int i10_host_recv_skb(read_descriptor_t *desc, struct sk_buff *skb,
			     unsigned int offset, size_t len)
{
	struct i10_host_queue *queue = desc->arg.data;
	size_t consumed = len;
	int result;

	while (len) {
		switch (i10_host_recv_state(queue)) {
		case NVME_TCP_RECV_PDU:
			result = i10_host_recv_pdu(queue, skb, &offset, &len);
			break;
		case NVME_TCP_RECV_DATA:
			result = i10_host_recv_data(queue, skb, &offset, &len);
			break;
		case NVME_TCP_RECV_DDGST:
			result = i10_host_recv_ddgst(queue, skb, &offset, &len);
			break;
		default:
			result = -EFAULT;
		}
		if (result) {
			dev_err(queue->ctrl->ctrl.device,
				"receive failed:  %d\n", result);
			queue->rd_enabled = false;
			i10_host_error_recovery(&queue->ctrl->ctrl);
			return result;
		}
	}

	return consumed;
}

static void i10_host_data_ready(struct sock *sk)
{
	struct i10_host_queue *queue;

	read_lock(&sk->sk_callback_lock);
	queue = sk->sk_user_data;
	if (likely(queue && queue->rd_enabled))
		queue_work_on(queue->io_cpu, i10_host_wq, &queue->io_work);
	read_unlock(&sk->sk_callback_lock);
}

static void i10_host_write_space(struct sock *sk)
{
	struct i10_host_queue *queue;

	read_lock_bh(&sk->sk_callback_lock);
	queue = sk->sk_user_data;
	if (likely(queue && sk_stream_is_writeable(sk))) {
		clear_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
		queue_work_on(queue->io_cpu, i10_host_wq, &queue->io_work);
	}
	read_unlock_bh(&sk->sk_callback_lock);
}

static void i10_host_state_change(struct sock *sk)
{
	struct i10_host_queue *queue;

	read_lock(&sk->sk_callback_lock);
	queue = sk->sk_user_data;
	if (!queue)
		goto done;

	switch (sk->sk_state) {
	case TCP_CLOSE:
	case TCP_CLOSE_WAIT:
	case TCP_LAST_ACK:
	case TCP_FIN_WAIT1:
	case TCP_FIN_WAIT2:
		/* fallthrough */
		i10_host_error_recovery(&queue->ctrl->ctrl);
		break;
	default:
		dev_info(queue->ctrl->ctrl.device,
			"queue %d socket state %d\n",
			i10_host_queue_id(queue), sk->sk_state);
	}

	queue->state_change(sk);
done:
	read_unlock(&sk->sk_callback_lock);
}

static inline void i10_host_done_send_req(struct i10_host_queue *queue)
{
	queue->request = NULL;
}

static void i10_host_fail_request(struct i10_host_request *req)
{
	i10_host_end_request(blk_mq_rq_from_pdu(req), NVME_SC_HOST_PATH_ERROR);
}

static inline bool i10_host_is_caravan_full(struct i10_host_queue *queue, int len)
{
	return (queue->caravan_len + len >= I10_CARAVAN_CAPACITY) ||
		(queue->nr_iovs >= I10_AGGREGATION_SIZE * 2) ||
		(queue->nr_mapped >= I10_AGGREGATION_SIZE);
}

static int i10_host_try_send_data(struct i10_host_request *req)
{
	struct i10_host_queue *queue = req->queue;

	while (true) {
		struct page *page = i10_host_req_cur_page(req);
		size_t offset = i10_host_req_cur_offset(req);
		size_t len = i10_host_req_cur_length(req);
		bool last = i10_host_pdu_last_send(req, len);
		int ret, flags = MSG_DONTWAIT;

		if (last && !queue->data_digest)
			flags |= MSG_EOR;
		else
			flags |= MSG_MORE;

		if (i10_host_legacy_path(req)) {
			/* can't zcopy slab pages */
			if (unlikely(PageSlab(page))) {
				ret = sock_no_sendpage(queue->sock, page, offset, len,
						flags);
			} else {
				ret = kernel_sendpage(queue->sock, page, offset, len,
						flags);
			}
		}
		else {
			if (i10_host_is_caravan_full(queue, len)) {
				queue->send_now = true;
				return 1;
			}
			/* Caravans: I/O data aggregation */
			queue->caravan_iovs[queue->nr_iovs].iov_base =
				kmap(page) + offset;
			queue->caravan_iovs[queue->nr_iovs++].iov_len = len;
			queue->caravan_mapped[queue->nr_mapped++] = page;
			queue->caravan_len += len;
			ret = len;
		}

		if (ret <= 0)
			return ret;

		i10_host_advance_req(req, ret);
		if (queue->data_digest)
			i10_host_ddgst_update(queue->snd_hash, page,
					offset, ret);

		/* fully successful last write*/
		if (last && ret == len) {
			if (queue->data_digest) {
				i10_host_ddgst_final(queue->snd_hash,
					&req->ddgst);
				req->state = I10_HOST_SEND_DDGST;
				req->offset = 0;
			} else {
				i10_host_done_send_req(queue);
			}
			return 1;
		}
	}
	return -EAGAIN;
}

static int i10_host_try_send_cmd_pdu(struct i10_host_request *req)
{
	struct i10_host_queue *queue = req->queue;
	struct nvme_tcp_cmd_pdu *pdu = req->pdu;
	bool inline_data = i10_host_has_inline_data(req);
	int flags = MSG_DONTWAIT | (inline_data ? MSG_MORE : MSG_EOR);
	u8 hdgst = i10_host_hdgst_len(queue);
	int len = sizeof(*pdu) + hdgst - req->offset;
	int ret;

	if (queue->hdr_digest && !req->offset)
		i10_host_hdgst(queue->snd_hash, pdu, sizeof(*pdu));

	if (i10_host_legacy_path(req))
		ret = kernel_sendpage(queue->sock, virt_to_page(pdu),
			offset_in_page(pdu) + req->offset, len, flags);
	else {
		if (i10_host_is_caravan_full(queue, len)) {
			queue->send_now = true;
			return 1;
		}
		/* Caravans: command PDU aggregation */
		queue->caravan_iovs[queue->nr_iovs].iov_base = pdu
			+ req->offset;
		queue->caravan_iovs[queue->nr_iovs++].iov_len = len;
		queue->caravan_len += len;
		ret = len;

		if (i10_host_is_nodelay_path(req))
			queue->send_now = true;
	}

	if (unlikely(ret <= 0))
		return ret;

	len -= ret;
	if (!len) {
		if (inline_data) {
			req->state = I10_HOST_SEND_DATA;
			if (queue->data_digest)
				crypto_ahash_init(queue->snd_hash);
			i10_host_init_iter(req, WRITE);
		} else {
			i10_host_done_send_req(queue);
		}
		return 1;
	}
	req->offset += ret;

	return -EAGAIN;
}

static int i10_host_try_send_data_pdu(struct i10_host_request *req)
{
	struct i10_host_queue *queue = req->queue;
	struct nvme_tcp_data_pdu *pdu = req->pdu;
	u8 hdgst = i10_host_hdgst_len(queue);
	int len = sizeof(*pdu) - req->offset + hdgst;
	int ret;

	if (queue->hdr_digest && !req->offset)
		i10_host_hdgst(queue->snd_hash, pdu, sizeof(*pdu));

	if (i10_host_legacy_path(req))
		ret = kernel_sendpage(queue->sock, virt_to_page(pdu),
			offset_in_page(pdu) + req->offset, len,
			MSG_DONTWAIT | MSG_MORE);
	else {
		if (i10_host_is_caravan_full(queue, len)) {
			queue->send_now = true;
			return 1;
		}
		/* Caravans: data PDU aggregation */
		queue->caravan_iovs[queue->nr_iovs].iov_base = pdu
			+ req->offset;
		queue->caravan_iovs[queue->nr_iovs++].iov_len = len;
		queue->caravan_len += len;
		ret = len;
	}

	if (unlikely(ret <= 0))
		return ret;

	len -= ret;
	if (!len) {
		req->state = I10_HOST_SEND_DATA;
		if (queue->data_digest)
			crypto_ahash_init(queue->snd_hash);
		if (!req->data_sent)
			i10_host_init_iter(req, WRITE);
		return 1;
	}
	req->offset += ret;

	return -EAGAIN;
}

static int i10_host_try_send_ddgst(struct i10_host_request *req)
{
	struct i10_host_queue *queue = req->queue;
	int ret;
	struct msghdr msg = { .msg_flags = MSG_DONTWAIT | MSG_EOR };
	struct kvec iov = {
		.iov_base = &req->ddgst + req->offset,
		.iov_len = NVME_TCP_DIGEST_LENGTH - req->offset
	};

	ret = kernel_sendmsg(queue->sock, &msg, &iov, 1, iov.iov_len);
	if (unlikely(ret <= 0))
		return ret;

	if (req->offset + ret == NVME_TCP_DIGEST_LENGTH) {
		i10_host_done_send_req(queue);
		return 1;
	}

	req->offset += ret;
	return -EAGAIN;
}

/* To check if there's enough room in tcp_sndbuf */
static inline int i10_host_sndbuf_nospace(struct i10_host_queue *queue,
		int length)
{
	return sk_stream_wspace(queue->sock->sk) < length;
}

static bool i10_host_send_caravan(struct i10_host_queue *queue)
{
	/* 1. Caravan becomes full (64KB), or
	 * 2. No-delay request arrives, or
	 * 3. No more request remains in i10 queue
	 */
	return queue->send_now ||
		(queue->doorbell_expire && 
		!queue->request && queue->caravan_len);
}	

static int i10_host_try_send(struct i10_host_queue *queue)
{
	struct i10_host_request *req;
	int ret = 1;

	if (!queue->request) {
		queue->request = i10_host_fetch_request(queue);
		if (!queue->request && !queue->caravan_len)
			return 0;
	}

	/* Send i10 caravans now */
	if (i10_host_send_caravan(queue)) {
		struct msghdr msg = { .msg_flags = MSG_DONTWAIT | MSG_EOR };
		int i, i10_ret;

		if (i10_host_sndbuf_nospace(queue, queue->caravan_len)) {
			set_bit(SOCK_NOSPACE,
				&queue->sock->sk->sk_socket->flags);
			return 0;
		}

		i10_ret = kernel_sendmsg(queue->sock, &msg,
				queue->caravan_iovs,
				queue->nr_iovs,
				queue->caravan_len);

		if (unlikely(i10_ret <= 0))
			dev_err(queue->ctrl->ctrl.device,
				"I10_HOST: kernel_sendmsg fails (i10_ret %d)\n",
				i10_ret);

		for (i = 0; i < queue->nr_mapped; i++)
			kunmap(queue->caravan_mapped[i]);

		queue->nr_iovs = 0;
		queue->nr_mapped = 0;
		queue->caravan_len = 0;
		queue->send_now = false;
	}

	if (queue->request)
		req = queue->request;
	else
		return 0;

	if (req->state == I10_HOST_SEND_CMD_PDU) {
		ret = i10_host_try_send_cmd_pdu(req);
		if (ret <= 0)
			goto done;
		if (!i10_host_has_inline_data(req))
			return ret;
	}

	if (req->state == I10_HOST_SEND_H2C_PDU) {
		ret = i10_host_try_send_data_pdu(req);
		if (ret <= 0)
			goto done;
	}

	if (req->state == I10_HOST_SEND_DATA) {
		ret = i10_host_try_send_data(req);
		if (ret <= 0)
			goto done;
	}

	if (req->state == I10_HOST_SEND_DDGST)
		ret = i10_host_try_send_ddgst(req);
done:
	if (ret == -EAGAIN)
		ret = 0;
	return ret;
}

static int i10_host_try_recv(struct i10_host_queue *queue)
{
	struct socket *sock = queue->sock;
	struct sock *sk = sock->sk;
	read_descriptor_t rd_desc;
	int consumed;

	rd_desc.arg.data = queue;
	rd_desc.count = 1;
	lock_sock(sk);
	queue->nr_cqe = 0;
	consumed = sock->ops->read_sock(sk, &rd_desc, i10_host_recv_skb);
	release_sock(sk);
	return consumed;
}

enum hrtimer_restart i10_host_doorbell_timeout(struct hrtimer *timer)
{
	struct i10_host_queue *queue =
		container_of(timer, struct i10_host_queue,
			doorbell_timer);

	queue->doorbell_expire = true;
	queue->nr_req = 0;
	queue_work_on(queue->io_cpu, i10_host_wq, &queue->io_work);

	return HRTIMER_NORESTART;
}

static void i10_host_io_work(struct work_struct *w)
{
	struct i10_host_queue *queue =
		container_of(w, struct i10_host_queue, io_work);
	unsigned long deadline = jiffies + msecs_to_jiffies(1);

	do {
		bool pending = false;
		int result;

		result = i10_host_try_send(queue);
		if (result > 0) {
			pending = true;
		} else if (unlikely(result < 0)) {
			dev_err(queue->ctrl->ctrl.device,
				"failed to send request %d\n", result);
			if (result != -EPIPE)
				i10_host_fail_request(queue->request);
			i10_host_done_send_req(queue);
			return;
		}

		result = i10_host_try_recv(queue);
		if (result > 0)
			pending = true;

		if (!pending)
			return;

	} while (!time_after(jiffies, deadline)); /* quota is exhausted */

	queue_work_on(queue->io_cpu, i10_host_wq, &queue->io_work);
}

static void i10_host_free_crypto(struct i10_host_queue *queue)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(queue->rcv_hash);

	ahash_request_free(queue->rcv_hash);
	ahash_request_free(queue->snd_hash);
	crypto_free_ahash(tfm);
}

static int i10_host_alloc_crypto(struct i10_host_queue *queue)
{
	struct crypto_ahash *tfm;

	tfm = crypto_alloc_ahash("crc32c", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	queue->snd_hash = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!queue->snd_hash)
		goto free_tfm;
	ahash_request_set_callback(queue->snd_hash, 0, NULL, NULL);

	queue->rcv_hash = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!queue->rcv_hash)
		goto free_snd_hash;
	ahash_request_set_callback(queue->rcv_hash, 0, NULL, NULL);

	return 0;
free_snd_hash:
	ahash_request_free(queue->snd_hash);
free_tfm:
	crypto_free_ahash(tfm);
	return -ENOMEM;
}

static void i10_host_free_async_req(struct i10_host_ctrl *ctrl)
{
	struct i10_host_request *async = &ctrl->async_req;

	page_frag_free(async->pdu);
}

static int i10_host_alloc_async_req(struct i10_host_ctrl *ctrl)
{
	struct i10_host_queue *queue = &ctrl->queues[0];
	struct i10_host_request *async = &ctrl->async_req;
	u8 hdgst = i10_host_hdgst_len(queue);

	async->pdu = page_frag_alloc(&queue->pf_cache,
		sizeof(struct nvme_tcp_cmd_pdu) + hdgst,
		GFP_KERNEL | __GFP_ZERO);
	if (!async->pdu)
		return -ENOMEM;

	async->queue = &ctrl->queues[0];
	return 0;
}

static void i10_host_free_queue(struct nvme_ctrl *nctrl, int qid)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	struct i10_host_queue *queue = &ctrl->queues[qid];

	if (!test_and_clear_bit(NVME_TCP_Q_ALLOCATED, &queue->flags))
		return;

	if (queue->hdr_digest || queue->data_digest)
		i10_host_free_crypto(queue);

	sock_release(queue->sock);
	kfree(queue->pdu);
	kfree(queue->caravan_iovs);
	kfree(queue->caravan_mapped);
	hrtimer_cancel(&queue->doorbell_timer);
}

static int i10_host_init_connection(struct i10_host_queue *queue)
{
	struct nvme_tcp_icreq_pdu *icreq;
	struct nvme_tcp_icresp_pdu *icresp;
	struct msghdr msg = {};
	struct kvec iov;
	bool ctrl_hdgst, ctrl_ddgst;
	int ret;

	icreq = kzalloc(sizeof(*icreq), GFP_KERNEL);
	if (!icreq)
		return -ENOMEM;

	icresp = kzalloc(sizeof(*icresp), GFP_KERNEL);
	if (!icresp) {
		ret = -ENOMEM;
		goto free_icreq;
	}

	icreq->hdr.type = nvme_tcp_icreq;
	icreq->hdr.hlen = sizeof(*icreq);
	icreq->hdr.pdo = 0;
	icreq->hdr.plen = cpu_to_le32(icreq->hdr.hlen);
	icreq->pfv = cpu_to_le16(NVME_TCP_PFV_1_0);
	icreq->maxr2t = 0; /* single inflight r2t supported */
	icreq->hpda = 0; /* no alignment constraint */
	if (queue->hdr_digest)
		icreq->digest |= NVME_TCP_HDR_DIGEST_ENABLE;
	if (queue->data_digest)
		icreq->digest |= NVME_TCP_DATA_DIGEST_ENABLE;

	iov.iov_base = icreq;
	iov.iov_len = sizeof(*icreq);
	ret = kernel_sendmsg(queue->sock, &msg, &iov, 1, iov.iov_len);
	if (ret < 0)
		goto free_icresp;

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = icresp;
	iov.iov_len = sizeof(*icresp);
	ret = kernel_recvmsg(queue->sock, &msg, &iov, 1,
			iov.iov_len, msg.msg_flags);
	if (ret < 0)
		goto free_icresp;

	ret = -EINVAL;
	if (icresp->hdr.type != nvme_tcp_icresp) {
		pr_err("queue %d: bad type returned %d\n",
			i10_host_queue_id(queue), icresp->hdr.type);
		goto free_icresp;
	}

	if (le32_to_cpu(icresp->hdr.plen) != sizeof(*icresp)) {
		pr_err("queue %d: bad pdu length returned %d\n",
			i10_host_queue_id(queue), icresp->hdr.plen);
		goto free_icresp;
	}

	if (icresp->pfv != NVME_TCP_PFV_1_0) {
		pr_err("queue %d: bad pfv returned %d\n",
			i10_host_queue_id(queue), icresp->pfv);
		goto free_icresp;
	}

	ctrl_ddgst = !!(icresp->digest & NVME_TCP_DATA_DIGEST_ENABLE);
	if ((queue->data_digest && !ctrl_ddgst) ||
	    (!queue->data_digest && ctrl_ddgst)) {
		pr_err("queue %d: data digest mismatch host: %s ctrl: %s\n",
			i10_host_queue_id(queue),
			queue->data_digest ? "enabled" : "disabled",
			ctrl_ddgst ? "enabled" : "disabled");
		goto free_icresp;
	}

	ctrl_hdgst = !!(icresp->digest & NVME_TCP_HDR_DIGEST_ENABLE);
	if ((queue->hdr_digest && !ctrl_hdgst) ||
	    (!queue->hdr_digest && ctrl_hdgst)) {
		pr_err("queue %d: header digest mismatch host: %s ctrl: %s\n",
			i10_host_queue_id(queue),
			queue->hdr_digest ? "enabled" : "disabled",
			ctrl_hdgst ? "enabled" : "disabled");
		goto free_icresp;
	}

	if (icresp->cpda != 0) {
		pr_err("queue %d: unsupported cpda returned %d\n",
			i10_host_queue_id(queue), icresp->cpda);
		goto free_icresp;
	}

	ret = 0;
free_icresp:
	kfree(icresp);
free_icreq:
	kfree(icreq);
	return ret;
}

static int i10_host_alloc_queue(struct nvme_ctrl *nctrl,
		int qid, size_t queue_size)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	struct i10_host_queue *queue = &ctrl->queues[qid];
	struct linger sol = { .l_onoff = 1, .l_linger = 0 };
	int ret, opt, rcv_pdu_size;

	queue->ctrl = ctrl;
	INIT_LIST_HEAD(&queue->send_list);
	spin_lock_init(&queue->lock);
	INIT_WORK(&queue->io_work, i10_host_io_work);
	queue->queue_size = queue_size;

	if (qid > 0)
		queue->cmnd_capsule_len = ctrl->ctrl.ioccsz * 16;
	else
		queue->cmnd_capsule_len = sizeof(struct nvme_command) +
						NVME_TCP_ADMIN_CCSZ;

	ret = sock_create(ctrl->addr.ss_family, SOCK_STREAM,
			IPPROTO_TCP, &queue->sock);
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to create socket: %d\n", ret);
		return ret;
	}

	/* Single syn retry */
	opt = 1;
	ret = kernel_setsockopt(queue->sock, IPPROTO_TCP, TCP_SYNCNT,
			(char *)&opt, sizeof(opt));
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to set TCP_SYNCNT sock opt %d\n", ret);
		goto err_sock;
	}

	/* Set TCP no delay */
	opt = 1;
	ret = kernel_setsockopt(queue->sock, IPPROTO_TCP,
			TCP_NODELAY, (char *)&opt, sizeof(opt));
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to set TCP_NODELAY sock opt %d\n", ret);
		goto err_sock;
	}

	/* Set a fixed size of sndbuf/rcvbuf (8MB) */
	opt = 8388608;
	ret = kernel_setsockopt(queue->sock, SOL_SOCKET, SO_SNDBUFFORCE,
			(char *)&opt, sizeof(opt));
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to set SO_SNDBUFFORCE sock opt %d\n", ret);
		goto err_sock;
	}

	ret = kernel_setsockopt(queue->sock, SOL_SOCKET, SO_RCVBUFFORCE,
			(char *)&opt, sizeof(opt));
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to set SO_RCVBUFFORCE sock opt %d\n", ret);
		goto err_sock;
	}	

	/*
	 * Cleanup whatever is sitting in the TCP transmit queue on socket
	 * close. This is done to prevent stale data from being sent should
	 * the network connection be restored before TCP times out.
	 */
	ret = kernel_setsockopt(queue->sock, SOL_SOCKET, SO_LINGER,
			(char *)&sol, sizeof(sol));
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to set SO_LINGER sock opt %d\n", ret);
		goto err_sock;
	}

	queue->sock->sk->sk_allocation = GFP_ATOMIC;
	queue->io_cpu = (qid == 0) ? 0 : qid - 1;
	queue->request = NULL;
	queue->data_remaining = 0;
	queue->ddgst_remaining = 0;
	queue->pdu_remaining = 0;
	queue->pdu_offset = 0;
	sk_set_memalloc(queue->sock->sk);

	if (ctrl->ctrl.opts->mask & NVMF_OPT_HOST_TRADDR) {
		ret = kernel_bind(queue->sock, (struct sockaddr *)&ctrl->src_addr,
			sizeof(ctrl->src_addr));
		if (ret) {
			dev_err(ctrl->ctrl.device,
				"failed to bind queue %d socket %d\n",
				qid, ret);
			goto err_sock;
		}
	}

	/* i10 initialization */
	queue->caravan_iovs = kcalloc(I10_AGGREGATION_SIZE * 2,
				sizeof(*queue->caravan_iovs), GFP_KERNEL);
	if (!queue->caravan_iovs) {
		ret = -ENOMEM;
		goto err_sock;
	}

	queue->caravan_mapped = kcalloc(I10_AGGREGATION_SIZE,
				sizeof(**queue->caravan_mapped), GFP_KERNEL);
	if (!queue->caravan_mapped) {
		ret = -ENOMEM;
		goto err_caravan_iovs;
	}

	queue->nr_iovs = 0;
	queue->nr_req = 0;
	queue->nr_mapped = 0;
	queue->caravan_len = 0;
	queue->doorbell_expire = true;
	queue->send_now = false;

	/* i10 delayed doorbell setup */
	hrtimer_init(&queue->doorbell_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	queue->doorbell_timer.function = &i10_host_doorbell_timeout;

	queue->hdr_digest = nctrl->opts->hdr_digest;
	queue->data_digest = nctrl->opts->data_digest;
	if (queue->hdr_digest || queue->data_digest) {
		ret = i10_host_alloc_crypto(queue);
		if (ret) {
			dev_err(ctrl->ctrl.device,
				"failed to allocate queue %d crypto\n", qid);
			goto err_caravan_mapped;
		}
	}

	rcv_pdu_size = sizeof(struct nvme_tcp_rsp_pdu) +
			i10_host_hdgst_len(queue);
	queue->pdu = kmalloc(rcv_pdu_size, GFP_KERNEL);
	if (!queue->pdu) {
		ret = -ENOMEM;
		goto err_crypto;
	}

	dev_dbg(ctrl->ctrl.device, "connecting queue %d\n",
			i10_host_queue_id(queue));

	ret = kernel_connect(queue->sock, (struct sockaddr *)&ctrl->addr,
		sizeof(ctrl->addr), 0);
	if (ret) {
		dev_err(ctrl->ctrl.device,
			"failed to connect socket: %d\n", ret);
		goto err_rcv_pdu;
	}

	ret = i10_host_init_connection(queue);
	if (ret)
		goto err_init_connect;

	queue->rd_enabled = true;
	set_bit(NVME_TCP_Q_ALLOCATED, &queue->flags);
	i10_host_init_recv_ctx(queue);

	write_lock_bh(&queue->sock->sk->sk_callback_lock);
	queue->sock->sk->sk_user_data = queue;
	queue->state_change = queue->sock->sk->sk_state_change;
	queue->data_ready = queue->sock->sk->sk_data_ready;
	queue->write_space = queue->sock->sk->sk_write_space;
	queue->sock->sk->sk_data_ready = i10_host_data_ready;
	queue->sock->sk->sk_state_change = i10_host_state_change;
	queue->sock->sk->sk_write_space = i10_host_write_space;
#ifdef CONFIG_NET_RX_BUSY_POLL
	queue->sock->sk->sk_ll_usec = 1;
#endif
	write_unlock_bh(&queue->sock->sk->sk_callback_lock);

	return 0;

err_init_connect:
	kernel_sock_shutdown(queue->sock, SHUT_RDWR);
err_rcv_pdu:
	kfree(queue->pdu);
err_crypto:
	if (queue->hdr_digest || queue->data_digest)
		i10_host_free_crypto(queue);
err_caravan_mapped:
	kfree(queue->caravan_mapped);
err_caravan_iovs:
	kfree(queue->caravan_iovs);
err_sock:
	sock_release(queue->sock);
	queue->sock = NULL;
	return ret;
}

static void i10_host_restore_sock_calls(struct i10_host_queue *queue)
{
	struct socket *sock = queue->sock;

	write_lock_bh(&sock->sk->sk_callback_lock);
	sock->sk->sk_user_data  = NULL;
	sock->sk->sk_data_ready = queue->data_ready;
	sock->sk->sk_state_change = queue->state_change;
	sock->sk->sk_write_space  = queue->write_space;
	write_unlock_bh(&sock->sk->sk_callback_lock);
}

static void __i10_host_stop_queue(struct i10_host_queue *queue)
{
	kernel_sock_shutdown(queue->sock, SHUT_RDWR);
	i10_host_restore_sock_calls(queue);
	cancel_work_sync(&queue->io_work);
}

static void i10_host_stop_queue(struct nvme_ctrl *nctrl, int qid)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	struct i10_host_queue *queue = &ctrl->queues[qid];

	if (!test_and_clear_bit(NVME_TCP_Q_LIVE, &queue->flags))
		return;

	__i10_host_stop_queue(queue);
}

static int i10_host_start_queue(struct nvme_ctrl *nctrl, int idx)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	int ret;

	if (idx)
		ret = nvmf_connect_io_queue(nctrl, idx, false);
	else
		ret = nvmf_connect_admin_queue(nctrl);

	if (!ret) {
		set_bit(NVME_TCP_Q_LIVE, &ctrl->queues[idx].flags);
	} else {
		if (test_bit(NVME_TCP_Q_ALLOCATED, &ctrl->queues[idx].flags))
			__i10_host_stop_queue(&ctrl->queues[idx]);
		dev_err(nctrl->device,
			"failed to connect queue: %d ret=%d\n", idx, ret);
	}
	return ret;
}

static struct blk_mq_tag_set *i10_host_alloc_tagset(struct nvme_ctrl *nctrl,
		bool admin)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	struct blk_mq_tag_set *set;
	int ret;

	if (admin) {
		set = &ctrl->admin_tag_set;
		memset(set, 0, sizeof(*set));
		set->ops = &i10_host_admin_mq_ops;
		set->queue_depth = NVME_AQ_MQ_TAG_DEPTH;
		set->reserved_tags = 2; /* connect + keep-alive */
		set->numa_node = NUMA_NO_NODE;
		set->cmd_size = sizeof(struct i10_host_request);
		set->driver_data = ctrl;
		set->nr_hw_queues = 1;
		set->timeout = ADMIN_TIMEOUT;
	} else {
		set = &ctrl->tag_set;
		memset(set, 0, sizeof(*set));
		set->ops = &i10_host_mq_ops;
		set->queue_depth = nctrl->sqsize + 1;
		set->reserved_tags = 1; /* fabric connect */
		set->numa_node = NUMA_NO_NODE;
		set->flags = BLK_MQ_F_SHOULD_MERGE;
		set->cmd_size = sizeof(struct i10_host_request);
		set->driver_data = ctrl;
		set->nr_hw_queues = nctrl->queue_count - 1;
		set->timeout = NVME_IO_TIMEOUT;
		set->nr_maps = nctrl->opts->nr_poll_queues ? HCTX_MAX_TYPES : 2;
	}

	ret = blk_mq_alloc_tag_set(set);
	if (ret)
		return ERR_PTR(ret);

	return set;
}

static void i10_host_free_admin_queue(struct nvme_ctrl *ctrl)
{
	if (to_i10_host_ctrl(ctrl)->async_req.pdu) {
		i10_host_free_async_req(to_i10_host_ctrl(ctrl));
		to_i10_host_ctrl(ctrl)->async_req.pdu = NULL;
	}

	i10_host_free_queue(ctrl, 0);
}

static void i10_host_free_io_queues(struct nvme_ctrl *ctrl)
{
	int i;

	for (i = 1; i < ctrl->queue_count; i++)
		i10_host_free_queue(ctrl, i);
}

static void i10_host_stop_io_queues(struct nvme_ctrl *ctrl)
{
	int i;

	for (i = 1; i < ctrl->queue_count; i++)
		i10_host_stop_queue(ctrl, i);
}

static int i10_host_start_io_queues(struct nvme_ctrl *ctrl)
{
	int i, ret = 0;

	for (i = 1; i < ctrl->queue_count; i++) {
		ret = i10_host_start_queue(ctrl, i);
		if (ret)
			goto out_stop_queues;
	}

	return 0;

out_stop_queues:
	for (i--; i >= 1; i--)
		i10_host_stop_queue(ctrl, i);
	return ret;
}

static int i10_host_alloc_admin_queue(struct nvme_ctrl *ctrl)
{
	int ret;

	ret = i10_host_alloc_queue(ctrl, 0, NVME_AQ_DEPTH);
	if (ret)
		return ret;

	ret = i10_host_alloc_async_req(to_i10_host_ctrl(ctrl));
	if (ret)
		goto out_free_queue;

	return 0;

out_free_queue:
	i10_host_free_queue(ctrl, 0);
	return ret;
}

static int __i10_host_alloc_io_queues(struct nvme_ctrl *ctrl)
{
	int i, ret;

	for (i = 1; i < ctrl->queue_count; i++) {
		ret = i10_host_alloc_queue(ctrl, i,
				ctrl->sqsize + 1);
		if (ret)
			goto out_free_queues;
	}

	return 0;

out_free_queues:
	for (i--; i >= 1; i--)
		i10_host_free_queue(ctrl, i);

	return ret;
}

static unsigned int i10_host_nr_io_queues(struct nvme_ctrl *ctrl)
{
	unsigned int nr_io_queues;

	nr_io_queues = min(ctrl->opts->nr_io_queues, num_online_cpus());
	nr_io_queues += min(ctrl->opts->nr_write_queues, num_online_cpus());
	nr_io_queues += min(ctrl->opts->nr_poll_queues, num_online_cpus());

	return nr_io_queues;
}

static void i10_host_set_io_queues(struct nvme_ctrl *nctrl,
		unsigned int nr_io_queues)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);
	struct nvmf_ctrl_options *opts = nctrl->opts;

	if (opts->nr_write_queues && opts->nr_io_queues < nr_io_queues) {
		/*
		 * separate read/write queues
		 * hand out dedicated default queues only after we have
		 * sufficient read queues.
		 */
		ctrl->io_queues[HCTX_TYPE_READ] = opts->nr_io_queues;
		nr_io_queues -= ctrl->io_queues[HCTX_TYPE_READ];
		ctrl->io_queues[HCTX_TYPE_DEFAULT] =
			min(opts->nr_write_queues, nr_io_queues);
		nr_io_queues -= ctrl->io_queues[HCTX_TYPE_DEFAULT];
	} else {
		/*
		 * shared read/write queues
		 * either no write queues were requested, or we don't have
		 * sufficient queue count to have dedicated default queues.
		 */
		ctrl->io_queues[HCTX_TYPE_DEFAULT] =
			min(opts->nr_io_queues, nr_io_queues);
		nr_io_queues -= ctrl->io_queues[HCTX_TYPE_DEFAULT];
	}

	if (opts->nr_poll_queues && nr_io_queues) {
		/* map dedicated poll queues only if we have queues left */
		ctrl->io_queues[HCTX_TYPE_POLL] =
			min(opts->nr_poll_queues, nr_io_queues);
	}
}

static int i10_host_alloc_io_queues(struct nvme_ctrl *ctrl)
{
	unsigned int nr_io_queues;
	int ret;

	nr_io_queues = i10_host_nr_io_queues(ctrl);
	ret = nvme_set_queue_count(ctrl, &nr_io_queues);
	if (ret)
		return ret;

	ctrl->queue_count = nr_io_queues + 1;
	if (ctrl->queue_count < 2)
		return 0;

	dev_info(ctrl->device,
		"creating %d I/O queues.\n", nr_io_queues);

	i10_host_set_io_queues(ctrl, nr_io_queues);

	return __i10_host_alloc_io_queues(ctrl);
}

static void i10_host_destroy_io_queues(struct nvme_ctrl *ctrl, bool remove)
{
	i10_host_stop_io_queues(ctrl);
	if (remove) {
		if (ctrl->ops->flags & NVME_F_FABRICS)
			blk_cleanup_queue(ctrl->connect_q);
		blk_mq_free_tag_set(ctrl->tagset);
	}
	i10_host_free_io_queues(ctrl);
}

static int i10_host_configure_io_queues(struct nvme_ctrl *ctrl, bool new)
{
	int ret;

	ret = i10_host_alloc_io_queues(ctrl);
	if (ret)
		return ret;

	if (new) {
		ctrl->tagset = i10_host_alloc_tagset(ctrl, false);
		if (IS_ERR(ctrl->tagset)) {
			ret = PTR_ERR(ctrl->tagset);
			goto out_free_io_queues;
		}

		ctrl->connect_q = blk_mq_init_queue(ctrl->tagset);
		if (IS_ERR(ctrl->connect_q)) {
			ret = PTR_ERR(ctrl->connect_q);
			goto out_free_tag_set;
		}
	} else {
		blk_mq_update_nr_hw_queues(ctrl->tagset,
			ctrl->queue_count - 1);
	}

	ret = i10_host_start_io_queues(ctrl);
	if (ret)
		goto out_cleanup_connect_q;

	return 0;

out_cleanup_connect_q:
	if (new)
		blk_cleanup_queue(ctrl->connect_q);
out_free_tag_set:
	if (new)
		blk_mq_free_tag_set(ctrl->tagset);
out_free_io_queues:
	i10_host_free_io_queues(ctrl);
	return ret;
}

static void i10_host_destroy_admin_queue(struct nvme_ctrl *ctrl, bool remove)
{
	i10_host_stop_queue(ctrl, 0);
	if (remove) {
		free_opal_dev(ctrl->opal_dev);
		blk_cleanup_queue(ctrl->admin_q);
		blk_mq_free_tag_set(ctrl->admin_tagset);
	}
	i10_host_free_admin_queue(ctrl);
}

static int i10_host_configure_admin_queue(struct nvme_ctrl *ctrl, bool new)
{
	int error;

	error = i10_host_alloc_admin_queue(ctrl);
	if (error)
		return error;

	if (new) {
		ctrl->admin_tagset = i10_host_alloc_tagset(ctrl, true);
		if (IS_ERR(ctrl->admin_tagset)) {
			error = PTR_ERR(ctrl->admin_tagset);
			goto out_free_queue;
		}

		ctrl->admin_q = blk_mq_init_queue(ctrl->admin_tagset);
		if (IS_ERR(ctrl->admin_q)) {
			error = PTR_ERR(ctrl->admin_q);
			goto out_free_tagset;
		}

		ctrl->admin_q = blk_mq_init_queue(ctrl->admin_tagset);
		if (IS_ERR(ctrl->admin_q)) {
			error = PTR_ERR(ctrl->admin_q);
			goto out_cleanup_fabrics_q;
		}
	}

	error = i10_host_start_queue(ctrl, 0);
	if (error)
		goto out_cleanup_queue;

	error = nvme_enable_ctrl(ctrl);
	if (error)
		goto out_stop_queue;

	blk_mq_unquiesce_queue(ctrl->admin_q);

	error = nvme_init_identify(ctrl);
	if (error)
		goto out_stop_queue;

	return 0;

out_stop_queue:
	i10_host_stop_queue(ctrl, 0);
out_cleanup_queue:
	if (new)
		blk_cleanup_queue(ctrl->admin_q);
out_cleanup_fabrics_q:
	if (new)
		blk_cleanup_queue(ctrl->fabrics_q);
out_free_tagset:
	if (new)
		blk_mq_free_tag_set(ctrl->admin_tagset);
out_free_queue:
	i10_host_free_admin_queue(ctrl);
	return error;
}

static void i10_host_teardown_admin_queue(struct nvme_ctrl *ctrl,
		bool remove)
{
	blk_mq_quiesce_queue(ctrl->admin_q);
	i10_host_stop_queue(ctrl, 0);
	if (ctrl->admin_tagset) {
		blk_mq_tagset_busy_iter(ctrl->admin_tagset,
			nvme_cancel_request, ctrl);
		blk_mq_tagset_wait_completed_request(ctrl->admin_tagset);
	}
	if (remove)
		blk_mq_unquiesce_queue(ctrl->admin_q);
	i10_host_destroy_admin_queue(ctrl, remove);
}

static void i10_host_teardown_io_queues(struct nvme_ctrl *ctrl,
		bool remove)
{
	if (ctrl->queue_count <= 1)
		return;
	nvme_stop_queues(ctrl);
	i10_host_stop_io_queues(ctrl);
	if (ctrl->tagset) {
		blk_mq_tagset_busy_iter(ctrl->tagset,
			nvme_cancel_request, ctrl);
		blk_mq_tagset_wait_completed_request(ctrl->tagset);
	}
	if (remove)
		nvme_start_queues(ctrl);
	i10_host_destroy_io_queues(ctrl, remove);
}

static void i10_host_reconnect_or_remove(struct nvme_ctrl *ctrl)
{
	/* If we are resetting/deleting then do nothing */
	if (ctrl->state != NVME_CTRL_CONNECTING) {
		WARN_ON_ONCE(ctrl->state == NVME_CTRL_NEW ||
			ctrl->state == NVME_CTRL_LIVE);
		return;
	}

	if (nvmf_should_reconnect(ctrl)) {
		dev_info(ctrl->device, "Reconnecting in %d seconds...\n",
			ctrl->opts->reconnect_delay);
		queue_delayed_work(nvme_wq, &to_i10_host_ctrl(ctrl)->connect_work,
				ctrl->opts->reconnect_delay * HZ);
	} else {
		dev_info(ctrl->device, "Removing controller...\n");
		nvme_delete_ctrl(ctrl);
	}
}

static int i10_host_setup_ctrl(struct nvme_ctrl *ctrl, bool new)
{
	struct nvmf_ctrl_options *opts = ctrl->opts;
	int ret;

	ret = i10_host_configure_admin_queue(ctrl, new);
	if (ret)
		return ret;

	if (ctrl->icdoff) {
		dev_err(ctrl->device, "icdoff is not supported!\n");
		goto destroy_admin;
	}

	if (opts->queue_size > ctrl->sqsize + 1)
		dev_warn(ctrl->device,
			"queue_size %zu > ctrl sqsize %u, clamping down\n",
			opts->queue_size, ctrl->sqsize + 1);

	if (ctrl->sqsize + 1 > ctrl->maxcmd) {
		dev_warn(ctrl->device,
			"sqsize %u > ctrl maxcmd %u, clamping down\n",
			ctrl->sqsize + 1, ctrl->maxcmd);
		ctrl->sqsize = ctrl->maxcmd - 1;
	}

	if (ctrl->queue_count > 1) {
		ret = i10_host_configure_io_queues(ctrl, new);
		if (ret)
			goto destroy_admin;
	}

	if (!nvme_change_ctrl_state(ctrl, NVME_CTRL_LIVE)) {
		/* state change failure is ok if we're in DELETING state */
		WARN_ON_ONCE(ctrl->state != NVME_CTRL_DELETING);
		ret = -EINVAL;
		goto destroy_io;
	}

	nvme_start_ctrl(ctrl);
	return 0;

destroy_io:
	if (ctrl->queue_count > 1)
		i10_host_destroy_io_queues(ctrl, new);
destroy_admin:
	i10_host_stop_queue(ctrl, 0);
	i10_host_destroy_admin_queue(ctrl, new);
	return ret;
}

static void i10_host_reconnect_ctrl_work(struct work_struct *work)
{
	struct i10_host_ctrl *tcp_ctrl = container_of(to_delayed_work(work),
			struct i10_host_ctrl, connect_work);
	struct nvme_ctrl *ctrl = &tcp_ctrl->ctrl;

	++ctrl->nr_reconnects;

	if (i10_host_setup_ctrl(ctrl, false))
		goto requeue;

	dev_info(ctrl->device, "Successfully reconnected (%d attepmpt)\n",
			ctrl->nr_reconnects);

	ctrl->nr_reconnects = 0;

	return;

requeue:
	dev_info(ctrl->device, "Failed reconnect attempt %d\n",
			ctrl->nr_reconnects);
	i10_host_reconnect_or_remove(ctrl);
}

static void i10_host_error_recovery_work(struct work_struct *work)
{
	struct i10_host_ctrl *tcp_ctrl = container_of(work,
				struct i10_host_ctrl, err_work);
	struct nvme_ctrl *ctrl = &tcp_ctrl->ctrl;

	nvme_stop_keep_alive(ctrl);
	i10_host_teardown_io_queues(ctrl, false);
	/* unquiesce to fail fast pending requests */
	nvme_start_queues(ctrl);
	i10_host_teardown_admin_queue(ctrl, false);
	blk_mq_unquiesce_queue(ctrl->admin_q);

	if (!nvme_change_ctrl_state(ctrl, NVME_CTRL_CONNECTING)) {
		/* state change failure is ok if we're in DELETING state */
		WARN_ON_ONCE(ctrl->state != NVME_CTRL_DELETING);
		return;
	}

	i10_host_reconnect_or_remove(ctrl);
}

static void i10_host_teardown_ctrl(struct nvme_ctrl *ctrl, bool shutdown)
{
	cancel_work_sync(&to_i10_host_ctrl(ctrl)->err_work);
	cancel_delayed_work_sync(&to_i10_host_ctrl(ctrl)->connect_work);

	i10_host_teardown_io_queues(ctrl, shutdown);
	blk_mq_quiesce_queue(ctrl->admin_q);
	if (shutdown)
		nvme_shutdown_ctrl(ctrl);
	else
		nvme_disable_ctrl(ctrl);
	i10_host_teardown_admin_queue(ctrl, shutdown);
}

static void i10_host_delete_ctrl(struct nvme_ctrl *ctrl)
{
	i10_host_teardown_ctrl(ctrl, true);
}

static void nvme_reset_ctrl_work(struct work_struct *work)
{
	struct nvme_ctrl *ctrl =
		container_of(work, struct nvme_ctrl, reset_work);

	nvme_stop_ctrl(ctrl);
	i10_host_teardown_ctrl(ctrl, false);

	if (!nvme_change_ctrl_state(ctrl, NVME_CTRL_CONNECTING)) {
		/* state change failure is ok if we're in DELETING state */
		WARN_ON_ONCE(ctrl->state != NVME_CTRL_DELETING);
		return;
	}

	if (i10_host_setup_ctrl(ctrl, false))
		goto out_fail;

	return;

out_fail:
	++ctrl->nr_reconnects;
	i10_host_reconnect_or_remove(ctrl);
}

static void i10_host_free_ctrl(struct nvme_ctrl *nctrl)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(nctrl);

	if (list_empty(&ctrl->list))
		goto free_ctrl;

	mutex_lock(&i10_host_ctrl_mutex);
	list_del(&ctrl->list);
	mutex_unlock(&i10_host_ctrl_mutex);

	nvmf_free_options(nctrl->opts);
free_ctrl:
	kfree(ctrl->queues);
	kfree(ctrl);
}

static void i10_host_set_sg_null(struct nvme_command *c)
{
	struct nvme_sgl_desc *sg = &c->common.dptr.sgl;

	sg->addr = 0;
	sg->length = 0;
	sg->type = (NVME_TRANSPORT_SGL_DATA_DESC << 4) |
			NVME_SGL_FMT_TRANSPORT_A;
}

static void i10_host_set_sg_inline(struct i10_host_queue *queue,
		struct nvme_command *c, u32 data_len)
{
	struct nvme_sgl_desc *sg = &c->common.dptr.sgl;

	sg->addr = cpu_to_le64(queue->ctrl->ctrl.icdoff);
	sg->length = cpu_to_le32(data_len);
	sg->type = (NVME_SGL_FMT_DATA_DESC << 4) | NVME_SGL_FMT_OFFSET;
}

static void i10_host_set_sg_host_data(struct nvme_command *c,
		u32 data_len)
{
	struct nvme_sgl_desc *sg = &c->common.dptr.sgl;

	sg->addr = 0;
	sg->length = cpu_to_le32(data_len);
	sg->type = (NVME_TRANSPORT_SGL_DATA_DESC << 4) |
			NVME_SGL_FMT_TRANSPORT_A;
}

static void i10_host_submit_async_event(struct nvme_ctrl *arg)
{
	struct i10_host_ctrl *ctrl = to_i10_host_ctrl(arg);
	struct i10_host_queue *queue = &ctrl->queues[0];
	struct nvme_tcp_cmd_pdu *pdu = ctrl->async_req.pdu;
	struct nvme_command *cmd = &pdu->cmd;
	u8 hdgst = i10_host_hdgst_len(queue);

	memset(pdu, 0, sizeof(*pdu));
	pdu->hdr.type = nvme_tcp_cmd;
	if (queue->hdr_digest)
		pdu->hdr.flags |= NVME_TCP_F_HDGST;
	pdu->hdr.hlen = sizeof(*pdu);
	pdu->hdr.plen = cpu_to_le32(pdu->hdr.hlen + hdgst);

	cmd->common.opcode = nvme_admin_async_event;
	cmd->common.command_id = NVME_AQ_BLK_MQ_DEPTH;
	cmd->common.flags |= NVME_CMD_SGL_METABUF;
	i10_host_set_sg_null(cmd);

	ctrl->async_req.state = I10_HOST_SEND_CMD_PDU;
	ctrl->async_req.offset = 0;
	ctrl->async_req.curr_bio = NULL;
	ctrl->async_req.data_len = 0;

	i10_host_queue_request(&ctrl->async_req);
}

static enum blk_eh_timer_return
i10_host_timeout(struct request *rq, bool reserved)
{
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);
	struct i10_host_ctrl *ctrl = req->queue->ctrl;
	struct nvme_tcp_cmd_pdu *pdu = req->pdu;

	/*
	 * Restart the timer if a controller reset is already scheduled. Any
	 * timed out commands would be handled before entering the connecting
	 * state.
	 */
	if (ctrl->ctrl.state == NVME_CTRL_RESETTING)
		return BLK_EH_RESET_TIMER;

	dev_warn(ctrl->ctrl.device,
		"queue %d: timeout request %#x type %d\n",
		i10_host_queue_id(req->queue), rq->tag, pdu->hdr.type);

	if (ctrl->ctrl.state != NVME_CTRL_LIVE) {
		/*
		 * Teardown immediately if controller times out while starting
		 * or we are already started error recovery. all outstanding
		 * requests are completed on shutdown, so we return BLK_EH_DONE.
		 */
		flush_work(&ctrl->err_work);
		i10_host_teardown_io_queues(&ctrl->ctrl, false);
		i10_host_teardown_admin_queue(&ctrl->ctrl, false);
		return BLK_EH_DONE;
	}

	dev_warn(ctrl->ctrl.device, "starting error recovery\n");
	i10_host_error_recovery(&ctrl->ctrl);

	return BLK_EH_RESET_TIMER;
}

static blk_status_t i10_host_map_data(struct i10_host_queue *queue,
			struct request *rq)
{
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);
	struct nvme_tcp_cmd_pdu *pdu = req->pdu;
	struct nvme_command *c = &pdu->cmd;

	c->common.flags |= NVME_CMD_SGL_METABUF;

	if (!blk_rq_nr_phys_segments(rq))
		i10_host_set_sg_null(c);
	else if (rq_data_dir(rq) == WRITE &&
		req->data_len <= i10_host_inline_data_size(queue))
		i10_host_set_sg_inline(queue, c, req->data_len);
	else
		i10_host_set_sg_host_data(c, req->data_len);

	return 0;
}

static blk_status_t i10_host_setup_cmd_pdu(struct nvme_ns *ns,
		struct request *rq)
{
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);
	struct nvme_tcp_cmd_pdu *pdu = req->pdu;
	struct i10_host_queue *queue = req->queue;
	u8 hdgst = i10_host_hdgst_len(queue), ddgst = 0;
	blk_status_t ret;

	ret = nvme_setup_cmd(ns, rq, &pdu->cmd);
	if (ret)
		return ret;

	req->state = I10_HOST_SEND_CMD_PDU;
	req->offset = 0;
	req->data_sent = 0;
	req->pdu_len = 0;
	req->pdu_sent = 0;
	req->data_len = blk_rq_nr_phys_segments(rq) ?
				blk_rq_payload_bytes(rq) : 0;
	req->curr_bio = rq->bio;

	if (rq_data_dir(rq) == WRITE &&
	    req->data_len <= i10_host_inline_data_size(queue))
		req->pdu_len = req->data_len;
	else if (req->curr_bio)
		i10_host_init_iter(req, READ);

	pdu->hdr.type = nvme_tcp_cmd;
	pdu->hdr.flags = 0;
	if (queue->hdr_digest)
		pdu->hdr.flags |= NVME_TCP_F_HDGST;
	if (queue->data_digest && req->pdu_len) {
		pdu->hdr.flags |= NVME_TCP_F_DDGST;
		ddgst = i10_host_ddgst_len(queue);
	}
	pdu->hdr.hlen = sizeof(*pdu);
	pdu->hdr.pdo = req->pdu_len ? pdu->hdr.hlen + hdgst : 0;
	pdu->hdr.plen =
		cpu_to_le32(pdu->hdr.hlen + hdgst + req->pdu_len + ddgst);

	ret = i10_host_map_data(queue, rq);
	if (unlikely(ret)) {
		nvme_cleanup_cmd(rq);
		dev_err(queue->ctrl->ctrl.device,
			"Failed to map data (%d)\n", ret);
		return ret;
	}

	return 0;
}

static blk_status_t i10_host_queue_rq(struct blk_mq_hw_ctx *hctx,
		const struct blk_mq_queue_data *bd)
{
	struct nvme_ns *ns = hctx->queue->queuedata;
	struct i10_host_queue *queue = hctx->driver_data;
	struct request *rq = bd->rq;
	struct i10_host_request *req = blk_mq_rq_to_pdu(rq);
	bool queue_ready = test_bit(NVME_TCP_Q_LIVE, &queue->flags);
	blk_status_t ret;

	if (!nvmf_check_ready(&queue->ctrl->ctrl, rq, queue_ready))
		return nvmf_fail_nonready_command(&queue->ctrl->ctrl, rq);

	ret = i10_host_setup_cmd_pdu(ns, rq);
	if (unlikely(ret))
		return ret;

	blk_mq_start_request(rq);

	i10_host_queue_request(req);

	return BLK_STS_OK;
}

static int i10_host_map_queues(struct blk_mq_tag_set *set)
{
	struct i10_host_ctrl *ctrl = set->driver_data;
	struct nvmf_ctrl_options *opts = ctrl->ctrl.opts;

	if (opts->nr_write_queues && ctrl->io_queues[HCTX_TYPE_READ]) {
		/* separate read/write queues */
		set->map[HCTX_TYPE_DEFAULT].nr_queues =
			ctrl->io_queues[HCTX_TYPE_DEFAULT];
		set->map[HCTX_TYPE_DEFAULT].queue_offset = 0;
		set->map[HCTX_TYPE_READ].nr_queues =
			ctrl->io_queues[HCTX_TYPE_READ];
		set->map[HCTX_TYPE_READ].queue_offset =
			ctrl->io_queues[HCTX_TYPE_DEFAULT];
	} else {
		/* shared read/write queues */
		set->map[HCTX_TYPE_DEFAULT].nr_queues =
			ctrl->io_queues[HCTX_TYPE_DEFAULT];
		set->map[HCTX_TYPE_DEFAULT].queue_offset = 0;
		set->map[HCTX_TYPE_READ].nr_queues =
			ctrl->io_queues[HCTX_TYPE_DEFAULT];
		set->map[HCTX_TYPE_READ].queue_offset = 0;
	}
	blk_mq_map_queues(&set->map[HCTX_TYPE_DEFAULT]);
	blk_mq_map_queues(&set->map[HCTX_TYPE_READ]);

	if (opts->nr_poll_queues && ctrl->io_queues[HCTX_TYPE_POLL]) {
		/* map dedicated poll queues only if we have queues left */
		set->map[HCTX_TYPE_POLL].nr_queues =
				ctrl->io_queues[HCTX_TYPE_POLL];
		set->map[HCTX_TYPE_POLL].queue_offset =
			ctrl->io_queues[HCTX_TYPE_DEFAULT] +
			ctrl->io_queues[HCTX_TYPE_READ];
		blk_mq_map_queues(&set->map[HCTX_TYPE_POLL]);
	}

	dev_info(ctrl->ctrl.device,
		"mapped %d/%d/%d default/read/poll queues.\n",
		ctrl->io_queues[HCTX_TYPE_DEFAULT],
		ctrl->io_queues[HCTX_TYPE_READ],
		ctrl->io_queues[HCTX_TYPE_POLL]);

	return 0;
}

static int i10_host_poll(struct blk_mq_hw_ctx *hctx)
{
	struct i10_host_queue *queue = hctx->driver_data;
	struct sock *sk = queue->sock->sk;

	if (sk_can_busy_loop(sk) && skb_queue_empty_lockless(&sk->sk_receive_queue))
		sk_busy_loop(sk, true);
	i10_host_try_recv(queue);
	return queue->nr_cqe;
}

static struct blk_mq_ops i10_host_mq_ops = {
	.queue_rq	= i10_host_queue_rq,
	.complete	= nvme_complete_rq,
	.init_request	= i10_host_init_request,
	.exit_request	= i10_host_exit_request,
	.init_hctx	= i10_host_init_hctx,
	.timeout	= i10_host_timeout,
	.map_queues	= i10_host_map_queues,
	.poll		= i10_host_poll,
};

static struct blk_mq_ops i10_host_admin_mq_ops = {
	.queue_rq	= i10_host_queue_rq,
	.complete	= nvme_complete_rq,
	.init_request	= i10_host_init_request,
	.exit_request	= i10_host_exit_request,
	.init_hctx	= i10_host_init_admin_hctx,
	.timeout	= i10_host_timeout,
};

static const struct nvme_ctrl_ops i10_host_ctrl_ops = {
	.name			= "i10",
	.module			= THIS_MODULE,
	.flags			= NVME_F_FABRICS,
	.reg_read32		= nvmf_reg_read32,
	.reg_read64		= nvmf_reg_read64,
	.reg_write32		= nvmf_reg_write32,
	.free_ctrl		= i10_host_free_ctrl,
	.submit_async_event	= i10_host_submit_async_event,
	.delete_ctrl		= i10_host_delete_ctrl,
	.get_address		= nvmf_get_address,
};

static bool
i10_host_existing_controller(struct nvmf_ctrl_options *opts)
{
	struct i10_host_ctrl *ctrl;
	bool found = false;

	mutex_lock(&i10_host_ctrl_mutex);
	list_for_each_entry(ctrl, &i10_host_ctrl_list, list) {
		found = nvmf_ip_options_match(&ctrl->ctrl, opts);
		if (found)
			break;
	}
	mutex_unlock(&i10_host_ctrl_mutex);

	return found;
}

static struct nvme_ctrl *i10_host_create_ctrl(struct device *dev,
		struct nvmf_ctrl_options *opts)
{
	struct i10_host_ctrl *ctrl;
	int ret;

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ctrl->list);
	ctrl->ctrl.opts = opts;
	ctrl->ctrl.queue_count = opts->nr_io_queues + opts->nr_write_queues +
				opts->nr_poll_queues + 1;
	ctrl->ctrl.sqsize = opts->queue_size - 1;
	ctrl->ctrl.kato = opts->kato;

	INIT_DELAYED_WORK(&ctrl->connect_work,
			i10_host_reconnect_ctrl_work);
	INIT_WORK(&ctrl->err_work, i10_host_error_recovery_work);
	INIT_WORK(&ctrl->ctrl.reset_work, nvme_reset_ctrl_work);

	if (!(opts->mask & NVMF_OPT_TRSVCID)) {
		opts->trsvcid =
			kstrdup(__stringify(NVME_TCP_DISC_PORT), GFP_KERNEL);
		if (!opts->trsvcid) {
			ret = -ENOMEM;
			goto out_free_ctrl;
		}
		opts->mask |= NVMF_OPT_TRSVCID;
	}

	ret = inet_pton_with_scope(&init_net, AF_UNSPEC,
			opts->traddr, opts->trsvcid, &ctrl->addr);
	if (ret) {
		pr_err("malformed address passed: %s:%s\n",
			opts->traddr, opts->trsvcid);
		goto out_free_ctrl;
	}

	if (opts->mask & NVMF_OPT_HOST_TRADDR) {
		ret = inet_pton_with_scope(&init_net, AF_UNSPEC,
			opts->host_traddr, NULL, &ctrl->src_addr);
		if (ret) {
			pr_err("malformed src address passed: %s\n",
			       opts->host_traddr);
			goto out_free_ctrl;
		}
	}

	if (!opts->duplicate_connect && i10_host_existing_controller(opts)) {
		ret = -EALREADY;
		goto out_free_ctrl;
	}

	ctrl->queues = kcalloc(ctrl->ctrl.queue_count, sizeof(*ctrl->queues),
				GFP_KERNEL);
	if (!ctrl->queues) {
		ret = -ENOMEM;
		goto out_free_ctrl;
	}

	ret = nvme_init_ctrl(&ctrl->ctrl, dev, &i10_host_ctrl_ops, 0);
	if (ret)
		goto out_kfree_queues;

	if (!nvme_change_ctrl_state(&ctrl->ctrl, NVME_CTRL_CONNECTING)) {
		WARN_ON_ONCE(1);
		ret = -EINTR;
		goto out_uninit_ctrl;
	}

	ret = i10_host_setup_ctrl(&ctrl->ctrl, true);
	if (ret)
		goto out_uninit_ctrl;

	dev_info(ctrl->ctrl.device, "new ctrl: NQN \"%s\", addr %pISp\n",
		ctrl->ctrl.opts->subsysnqn, &ctrl->addr);

	nvme_get_ctrl(&ctrl->ctrl);

	mutex_lock(&i10_host_ctrl_mutex);
	list_add_tail(&ctrl->list, &i10_host_ctrl_list);
	mutex_unlock(&i10_host_ctrl_mutex);

	return &ctrl->ctrl;

out_uninit_ctrl:
	nvme_uninit_ctrl(&ctrl->ctrl);
	nvme_put_ctrl(&ctrl->ctrl);
	if (ret > 0)
		ret = -EIO;
	return ERR_PTR(ret);
out_kfree_queues:
	kfree(ctrl->queues);
out_free_ctrl:
	kfree(ctrl);
	return ERR_PTR(ret);
}

static struct nvmf_transport_ops i10_host_transport = {
	.name		= "i10",
	.module		= THIS_MODULE,
	.required_opts	= NVMF_OPT_TRADDR,
	.allowed_opts	= NVMF_OPT_TRSVCID | NVMF_OPT_RECONNECT_DELAY |
			  NVMF_OPT_HOST_TRADDR | NVMF_OPT_CTRL_LOSS_TMO |
			  NVMF_OPT_HDR_DIGEST | NVMF_OPT_DATA_DIGEST |
			  NVMF_OPT_NR_WRITE_QUEUES | NVMF_OPT_NR_POLL_QUEUES |
			  NVMF_OPT_TOS,
	.create_ctrl	= i10_host_create_ctrl,
};

static int __init i10_host_init_module(void)
{
	i10_host_wq = alloc_workqueue("i10_host_wq",
			WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!i10_host_wq)
		return -ENOMEM;

	nvmf_register_transport(&i10_host_transport);
	return 0;
}

static void __exit i10_host_cleanup_module(void)
{
	struct i10_host_ctrl *ctrl;

	nvmf_unregister_transport(&i10_host_transport);

	mutex_lock(&i10_host_ctrl_mutex);
	list_for_each_entry(ctrl, &i10_host_ctrl_list, list)
		nvme_delete_ctrl(&ctrl->ctrl);
	mutex_unlock(&i10_host_ctrl_mutex);
	flush_workqueue(nvme_delete_wq);

	destroy_workqueue(i10_host_wq);
}

module_init(i10_host_init_module);
module_exit(i10_host_cleanup_module);

MODULE_LICENSE("GPL v2");
