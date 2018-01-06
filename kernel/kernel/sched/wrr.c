/*
 * Weight Round-Robin Scheduling Class (mapped to the SCHED_WRR policies)
 */

#include "sched.h"

#include <linux/slab.h>
#include <linux/irq_work.h>
#define WRR_TIMESLICE ((10 * HZ) / 1000)

#ifdef CONFIG_SMP

static int can_move_wrr_task(struct task_struct *p,
			     struct rq *source,
			     struct rq *target)
{
	if (!cpumask_test_cpu(target->cpu, tsk_cpus_allowed(p)))
		return 0;
	if (!cpu_online(target->cpu))
		return 0;
	if (task_cpu(p) != source->cpu)
		return 0;
	if (task_running(source, p))
		return 0;
	return 1;
}

#endif

static void
enqueue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se, bool head)
{
	struct list_head *queue = &rq->wrr.queue;

	if (head)
		list_add(&wrr_se->run_list, queue);
	else
		list_add_tail(&wrr_se->run_list, queue);
	rq->wrr.total_weights += wrr_se->weight;
	rq->wrr.wrr_nr_running++;
}

static void dequeue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se)
{
	list_del_init(&wrr_se->run_list);
	rq->wrr.total_weights -= wrr_se->weight;
	rq->wrr.wrr_nr_running--;
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct list_head *queue = &rq->wrr.queue;

	if (head)
		list_move(&wrr_se->run_list, queue);
	else
		list_move_tail(&wrr_se->run_list, queue);
}

static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	if (curr->sched_class != &wrr_sched_class)
		return;
	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;
	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));
	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);
	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);
}

static void watchdog(struct rq *rq, struct task_struct *p)
{
	unsigned long soft, hard;

	soft = task_rlimit(p, RLIMIT_RTTIME);
	hard = task_rlimit_max(p, RLIMIT_RTTIME);

	if (soft != RLIM_INFINITY) {
		unsigned long next;

		p->wrr.timeout++;
		next = DIV_ROUND_UP(min(soft, hard), USEC_PER_SEC / HZ);
		if (p->wrr.timeout > next)
			p->cputime_expires.sched_exp = p->se.sum_exec_runtime;
	}
}

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	INIT_LIST_HEAD(&wrr_rq->queue);
	wrr_rq->wrr_nr_running = 0;
	wrr_rq->total_weights = 0;
}

static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &(p->wrr);

	if (flags & ENQUEUE_WAKEUP)
		wrr_se->timeout = 0;
	enqueue_wrr_entity(rq, wrr_se, flags & ENQUEUE_HEAD);
	inc_nr_running(rq);
}

static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);
	dequeue_wrr_entity(rq, wrr_se);
	dec_nr_running(rq);
/*
 *#ifdef CONFIG_SMP
 *	if (rq->wrr.wrr_nr_running == 0)
 *		idle_balance_wrr(rq->cpu, rq);
 *#endif
 */
}

static void yield_task_wrr(struct rq *rq)
{
	requeue_task_wrr(rq, rq->curr, 0);
}

static void
check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	(void)rq;
	(void)p;
	(void)flags;
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
//	int ret = 0;
	struct sched_wrr_entity *head;
	struct task_struct *p;
	struct wrr_rq *wrr_rq  = &rq->wrr;

//again:
	if (unlikely(!wrr_rq->wrr_nr_running)) {
/*
 *#ifdef CONFIG_SMP
 *		ret = idle_balance_wrr(rq->cpu, rq);
 *		if (!ret)
 *			goto again;
 *#endif
 */
		return NULL;
	}
	head = list_first_entry(&rq->wrr.queue, struct sched_wrr_entity,
				run_list);
	p = container_of(head, struct task_struct, wrr);
	if (!p)
		return NULL;
	p->se.exec_start = rq->clock_task;
	return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	update_curr_wrr(rq);
	p->se.exec_start = 0;
}

#ifdef CONFIG_SMP

static int
select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	int min_cpu, origin_cpu;
	unsigned long min_weight, origin_weight, i;

	origin_cpu = task_cpu(p);
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK)
		return origin_cpu;
	origin_weight = cpu_rq(origin_cpu)->wrr.total_weights;
	min_weight = origin_weight;
	min_cpu = origin_cpu;
	rcu_read_lock();
	for_each_online_cpu(i) {
		struct wrr_rq *wrr_rq = &cpu_rq(i)->wrr;

		if (!cpumask_test_cpu(i, &p->cpus_allowed))
			continue;
		if (wrr_rq->total_weights < min_weight) {
			min_weight = wrr_rq->total_weights;
			min_cpu = i;
		}
	}
	rcu_read_unlock();
	return min_cpu;
}

#endif

static void set_curr_task_wrr(struct rq *rq)
{
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct list_head *queue = &rq->wrr.queue;

	update_curr_wrr(rq);
	watchdog(rq, p);
	if (p->policy != SCHED_WRR)
		return;
	if (--p->wrr.time_slice)
		return;
	if (p->wrr.weight > 1) {
		p->wrr.weight--;
		rq->wrr.total_weights--;
	}
	p->wrr.time_slice = WRR_TIMESLICE * p->wrr.weight;
	if (queue->prev != queue->next) {
		requeue_task_wrr(rq, p, 0);
		resched_task(p);
	}
}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
	(void)rq;
	(void)p;
	(void)oldprio;
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
	if (p->on_rq && rq->curr != p)
		if (rq == task_rq(p) && !rt_task(rq->curr))
			resched_task(rq->curr);
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	return WRR_TIMESLICE;
}

const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	.check_preempt_curr	= check_preempt_curr_wrr,
	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,
#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
#endif
	.set_curr_task		= set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.get_rr_interval	= get_rr_interval_wrr,
	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};

void idle_balance_wrr(int dest_cpu, struct rq *target_rq)
{
	unsigned long i;
	struct rq *source_rq;
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;

	raw_spin_unlock(&target_rq->lock);
	for_each_possible_cpu(i) {
		source_rq = cpu_rq(i);
		if (target_rq == source_rq)
			continue;
		double_rq_lock(source_rq, target_rq);
		if (source_rq->wrr.wrr_nr_running < 2) {
			double_rq_unlock(source_rq, target_rq);
			continue;
		}
		if (target_rq->wrr.wrr_nr_running > 0) {
			double_rq_unlock(source_rq, target_rq);
			break;
		}
		list_for_each_entry_reverse(wrr_se,
				&source_rq->wrr.queue, run_list) {
			p = container_of(wrr_se, struct task_struct, wrr);
			if (!can_move_wrr_task(p, source_rq, target_rq)
			    || p->policy != SCHED_WRR)
				continue;
			deactivate_task(source_rq, p, 0);
			set_task_cpu(p, dest_cpu);
			activate_task(target_rq, p, 0);
			check_preempt_curr(target_rq, p, 0);
			double_rq_unlock(source_rq, target_rq);
			goto quit;
		}
		double_rq_unlock(source_rq, target_rq);
	}
quit:
	raw_spin_lock(&target_rq->lock);
}
