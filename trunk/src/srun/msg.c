/****************************************************************************\
 *  msg.c - process message traffic between srun and slurm daemons
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <mgrondona@llnl.gov>, et. al.
 *  UCRL-CODE-2002-040.
 *  
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *  
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#if HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <time.h>

#include <slurm/slurm_errno.h>

#include "src/common/fd.h"
#include "src/common/hostlist.h"
#include "src/common/log.h"
#include "src/common/slurm_auth.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_defs.h"
#include "src/common/xassert.h"
#include "src/common/xmalloc.h"

#include "src/srun/job.h"
#include "src/srun/opt.h"
#include "src/srun/io.h"
#include "src/srun/signals.h"
#include "src/srun/sigstr.h"

#ifdef HAVE_TOTALVIEW
#  include "src/srun/attach.h"
#endif

#include "src/common/xstring.h"

#define LAUNCH_WAIT_SEC	 60	/* max wait to confirm launches, sec */

static int    tasks_exited     = 0;
static uid_t  slurm_uid;


/*
 *  Static prototypes
 */
static void	_accept_msg_connection(job_t *job, int fdnum);
static void	_confirm_launch_complete(job_t *job);
static void 	_exit_handler(job_t *job, slurm_msg_t *exit_msg);
static void	_handle_msg(job_t *job, slurm_msg_t *msg);
static inline bool _job_msg_done(job_t *job);
static void	_launch_handler(job_t *job, slurm_msg_t *resp);
static void     _do_poll_timeout(job_t *job);
static int      _get_next_timeout(job_t *job);
static void 	_msg_thr_poll(job_t *job);
static void	_set_jfds_nonblocking(job_t *job);
static void     _print_pid_list(const char *host, int ntasks, 
				uint32_t *pid, char *executable_name);

#define _poll_set_rd(_pfd, _fd) do {    \
	(_pfd).fd = _fd;                \
	(_pfd).events = POLLIN;         \
	} while (0)

#define _poll_set_wr(_pfd, _fd) do {    \
	(_pfd).fd = _fd;                \
	(_pfd).events = POLLOUT;        \
	} while (0)

#define _poll_rd_isset(pfd) ((pfd).revents & POLLIN )
#define _poll_wr_isset(pfd) ((pfd).revents & POLLOUT)
#define _poll_err(pfd)      ((pfd).revents & POLLERR)


#ifdef HAVE_TOTALVIEW
/*
 * Install entry in the MPI_proctable for host with node id `nodeid'
 *  and the number of tasks `ntasks' with pid array `pid'
 */
static void
_build_tv_list(job_t *job, char *host, int nodeid, int ntasks, uint32_t *pid)
{
	int i;
	static int tasks_recorded = 0;

	if (MPIR_proctable_size == 0) {
		MPIR_proctable_size = opt.nprocs;
		MPIR_proctable = xmalloc(sizeof(MPIR_PROCDESC) * opt.nprocs);
		totalview_jobid = NULL;
		xstrfmtcat(totalview_jobid, "%lu", job->jobid);
	}

	for (i = 0; i < ntasks; i++) {
		int taskid          = job->tids[nodeid][i];
		MPIR_PROCDESC *tv   = &MPIR_proctable[taskid];
		tv->host_name       = job->host[nodeid];
		tv->executable_name = remote_argv[0];
		tv->pid             = pid[i];

		tasks_recorded++;
	}

	if (tasks_recorded == opt.nprocs) {
		MPIR_debug_state = MPIR_DEBUG_SPAWNED;
		MPIR_Breakpoint(); 
	}
}


void tv_launch_failure(void)
{
	if (opt.totalview) {
		MPIR_debug_state = MPIR_DEBUG_ABORTING;
		MPIR_Breakpoint(); 
	}
}

void MPIR_Breakpoint(void)
{
	debug("In MPIR_Breakpoint");
	/* This just notifies TotalView that some event of interest occured */ 
}
#endif

static bool _job_msg_done(job_t *job)
{
	return (job->state >= SRUN_JOB_TERMINATED);
}

static void
_process_launch_resp(job_t *job, launch_tasks_response_msg_t *msg)
{
	if ((msg->srun_node_id < 0) || (msg->srun_node_id >= job->nhosts)) {
		error ("Bad launch response from %s", msg->node_name);
		return;
	}

	pthread_mutex_lock(&job->task_mutex);
	job->host_state[msg->srun_node_id] = SRUN_HOST_REPLIED;
	pthread_mutex_unlock(&job->task_mutex);
#ifdef HAVE_TOTALVIEW
	_build_tv_list( job, msg->node_name, msg->srun_node_id, 
			msg->count_of_pids,  msg->local_pids   );
#endif
	_print_pid_list( msg->node_name, msg->count_of_pids, 
			msg->local_pids, remote_argv[0]     );

}

static void
update_running_tasks(job_t *job, uint32_t nodeid)
{
	int i;
	debug2("updating %d running tasks for node %d", 
			job->ntask[nodeid], nodeid);
	slurm_mutex_lock(&job->task_mutex);
	for (i = 0; i < job->ntask[nodeid]; i++) {
		uint32_t tid = job->tids[nodeid][i];
		job->task_state[tid] = SRUN_TASK_RUNNING;
	}
	slurm_mutex_unlock(&job->task_mutex);
}

static void
update_failed_tasks(job_t *job, uint32_t nodeid)
{
	int i;
	slurm_mutex_lock(&job->task_mutex);
	for (i = 0; i < job->ntask[nodeid]; i++) {
		uint32_t tid = job->tids[nodeid][i];
		job->task_state[tid] = SRUN_TASK_FAILED;
		tasks_exited++;
	}
	slurm_mutex_unlock(&job->task_mutex);

	if (tasks_exited == opt.nprocs) {
		debug2("all tasks exited");
		update_job_state(job, SRUN_JOB_TERMINATED);
	}
}

static void
_launch_handler(job_t *job, slurm_msg_t *resp)
{
	launch_tasks_response_msg_t *msg = resp->data;

	debug2("received launch resp from %s nodeid=%d", msg->node_name,
			msg->srun_node_id);
	
	if (msg->return_code != 0)  {

		error("%s: launch failed: %s", 
		       msg->node_name, slurm_strerror(msg->return_code));

		slurm_mutex_lock(&job->task_mutex);
		job->host_state[msg->srun_node_id] = SRUN_HOST_REPLIED;
		slurm_mutex_unlock(&job->task_mutex);

		update_failed_tasks(job, msg->srun_node_id);

		/*
		if (!opt.no_kill) {
			job->rc = 124;
			update_job_state(job, SRUN_JOB_WAITING_ON_IO);
		} else 
			update_failed_tasks(job, msg->srun_node_id);
		*/
#ifdef HAVE_TOTALVIEW
		tv_launch_failure();
#endif
		return;
	} else {
		_process_launch_resp(job, msg);
		update_running_tasks(job, msg->srun_node_id);
	}
}

/* _confirm_launch_complete
 * confirm that all tasks registers a sucessful launch
 * pthread_exit with job kill on failure */
static void	
_confirm_launch_complete(job_t *job)
{
	int i;

	for (i=0; i<job->nhosts; i++) {
		if (job->host_state[i] != SRUN_HOST_REPLIED) {
			error ("Node %s not responding, terminating job step",
			       job->host[i]);
			job->rc = 124;
			update_job_state(job, SRUN_JOB_FAILED);
			pthread_exit(0);
		}
	}

	/*
	 *  Reset launch timeout so timer will no longer go off
	 */
	job->ltimeout = 0;
}

static void
_reattach_handler(job_t *job, slurm_msg_t *msg)
{
	int i;
	reattach_tasks_response_msg_t *resp = msg->data;

	if ((resp->srun_node_id < 0) || (resp->srun_node_id >= job->nhosts)) {
		error ("Invalid reattach response received");
		return;
	}

	slurm_mutex_lock(&job->task_mutex);
	job->host_state[resp->srun_node_id] = SRUN_HOST_REPLIED;
	slurm_mutex_unlock(&job->task_mutex);

	if (resp->return_code != 0) {
		if (job->stepid == NO_VAL) { 
			error ("Unable to attach to job %d: %s", 
			       job->jobid, slurm_strerror(resp->return_code));
		} else {
			error ("Unable to attach to step %d.%d on node %d: %s",
			       job->jobid, job->stepid, resp->srun_node_id,
			       slurm_strerror(resp->return_code));
		}
		job->rc = 1;
		update_job_state(job, SRUN_JOB_FAILED);
		return;
	}

	/* 
	 * store global task id information as returned from slurmd
	 */
	job->tids[resp->srun_node_id]  = 
		xmalloc( resp->ntasks * sizeof(uint32_t) );

	job->ntask[resp->srun_node_id] = resp->ntasks;      

	for (i = 0; i < resp->ntasks; i++) {
		job->tids[resp->srun_node_id][i] = resp->gids[i];
		job->hostid[resp->gids[i]]       = resp->srun_node_id;
	}

#if HAVE_TOTALVIEW
	if ((remote_argc == 0) && (resp->executable_name)) {
		remote_argc = 1;
		xrealloc(remote_argv, 2 * sizeof(char *));
		remote_argv[0] = resp->executable_name;
		resp->executable_name = NULL; /* nothing left to free */
		remote_argv[1] = NULL;
	}
	_build_tv_list(job, resp->node_name, resp->srun_node_id,
	                    resp->ntasks, resp->local_pids      );
#endif
	_print_pid_list(resp->node_name, resp->ntasks, resp->local_pids, 
			resp->executable_name);

	update_running_tasks(job, resp->srun_node_id);

}


static void 
_print_exit_status(job_t *job, hostlist_t hl, char *host, int status)
{
	char buf[1024];
	char *corestr = "";
	bool signaled  = false;
	void (*print) (const char *, ...) = (void *) &error; 

	xassert(hl != NULL);

	slurm_mutex_lock(&job->state_mutex);
	signaled = job->signaled;
	slurm_mutex_unlock(&job->state_mutex);

	/*
	 *  Print message that task was signaled as verbose message
	 *    not error message if the user generated the signal.
	 */
	if (signaled) 
		print = &verbose;

	hostlist_ranged_string(hl, sizeof(buf), buf);

	if (status == 0) {
		verbose("%s: %s: Done", host, buf);
		return;
	}

#ifdef WCOREDUMP
	if (WCOREDUMP(status))
		corestr = " (core dumped)";
#endif

	if (WIFSIGNALED(status))
		(*print) ("%s: %s: %s%s", host, buf, sigstr(status), corestr); 
	else
		error ("%s: %s: Exited with exit code %d", 
		       host, buf, WEXITSTATUS(status));

	return;
}

static void
_die_if_signaled(job_t *job, int status)
{
	bool signaled  = false;

	slurm_mutex_lock(&job->state_mutex);
	signaled = job->signaled;
	slurm_mutex_unlock(&job->state_mutex);

	if (WIFSIGNALED(status) && !signaled) {
		job->rc = 128 + WTERMSIG(status);
		update_job_state(job, SRUN_JOB_FAILED);
	}
}

static void 
_exit_handler(job_t *job, slurm_msg_t *exit_msg)
{
	task_exit_msg_t *msg       = (task_exit_msg_t *) exit_msg->data;
	hostlist_t       hl        = hostlist_create(NULL);
	int              hostid    = job->hostid[msg->task_id_list[0]]; 
	char            *host      = job->host[hostid];
	int              status    = msg->return_code;
	int              i;
	char             buf[1024];

	if (!job->etimeout && !tasks_exited) 
		job->etimeout = time(NULL) + opt.max_exit_timeout;

	for (i = 0; i < msg->num_tasks; i++) {
		uint32_t taskid = msg->task_id_list[i];

		if ((taskid < 0) || (taskid >= opt.nprocs)) {
			error("task exit resp has bad task id %d", taskid);
			continue;
		}

		snprintf(buf, sizeof(buf), "task%d", taskid);
		hostlist_push(hl, buf);

		slurm_mutex_lock(&job->task_mutex);
		job->tstatus[taskid] = status;
		if (status) 
			job->task_state[taskid] = SRUN_TASK_ABNORMAL_EXIT;
		else {
			if (   (job->err[taskid] != IO_DONE) 
			    || (job->out[taskid] != IO_DONE) )
				job->task_state[taskid] = SRUN_TASK_IO_WAIT;
			else
				job->task_state[taskid] = SRUN_TASK_EXITED;
		}
		slurm_mutex_unlock(&job->task_mutex);

		tasks_exited++;
		if (tasks_exited == opt.nprocs) {
			debug2("All tasks exited");
			update_job_state(job, SRUN_JOB_TERMINATED);
		}

	}

	_print_exit_status(job, hl, host, status);

	hostlist_destroy(hl);

	_die_if_signaled(job, status);

}

static void
_handle_msg(job_t *job, slurm_msg_t *msg)
{
	uid_t req_uid = g_slurm_auth_get_uid(msg->cred);
	uid_t uid     = getuid();

	if ((req_uid != slurm_uid) && (req_uid != 0) && (req_uid != uid)) {
		error ("Security violation, slurm message from uid %u", 
		       (unsigned int) req_uid);
		return;
	}

	switch (msg->msg_type)
	{
		case RESPONSE_LAUNCH_TASKS:
			_launch_handler(job, msg);
			slurm_free_launch_tasks_response_msg(msg->data);
			break;
		case MESSAGE_TASK_EXIT:
			_exit_handler(job, msg);
			slurm_free_task_exit_msg(msg->data);
			break;
		case RESPONSE_REATTACH_TASKS:
			debug2("recvd reattach response\n");
			_reattach_handler(job, msg);
			slurm_free_reattach_tasks_response_msg(msg->data);
			break;
		default:
			error("received spurious message type: %d\n",
					msg->msg_type);
			break;
	}
	slurm_free_msg(msg);
	return;
}

static void
_accept_msg_connection(job_t *job, int fdnum)
{
	slurm_fd fd;
	slurm_msg_t *msg = NULL;
	slurm_addr   cli_addr;
	char         host[256];
	short        port;

	if ((fd = slurm_accept_msg_conn(job->jfd[fdnum], &cli_addr)) < 0) {
		error("Unable to accept connection: %m");
		return;
	}

	slurm_get_addr(&cli_addr, &port, host, sizeof(host));
	debug2("got message connection from %s:%d", host, ntohs(port));

	msg = xmalloc(sizeof(*msg));
  again:
	if (slurm_receive_msg(fd, msg, 0) < 0) {
		if (errno == EINTR)
			goto again;
		error("slurm_receive_msg[%s]: %m", host);
		xfree(msg);
	} else {

		msg->conn_fd = fd;
		_handle_msg(job, msg); /* handle_msg frees msg */
	}

	slurm_close_accepted_conn(fd);
	return;
}

static void
_set_jfds_nonblocking(job_t *job)
{
	int i;
	for (i = 0; i < job->njfds; i++) 
		fd_set_nonblocking(job->jfd[i]);
}

/*
 *  Call poll() with a timeout. (timeout argument is in seconds)
 */
static int
_do_poll(job_t *job, struct pollfd *fds, int timeout)
{
	nfds_t nfds = job->njfds;
	int rc;

	while ((rc = poll(fds, nfds, timeout * 1000)) < 0) {
		switch (errno) {
		case EINTR:  continue;
		case ENOMEM:
		case EFAULT: fatal("poll: %m");
		default:     error("poll: %m. Continuing...");
			     continue;
		}
	}

	return rc;
}


/*
 *  Get the next timeout in seconds from now.
 */
static int 
_get_next_timeout(job_t *job)
{
	int timeout = -1;

	if (!job->ltimeout && !job->etimeout)
		return -1;

	if (!job->ltimeout)
		timeout = job->etimeout - time(NULL);
	else if (!job->etimeout)
		timeout = job->ltimeout - time(NULL);
	else 
		timeout = job->ltimeout < job->etimeout ? 
			  job->ltimeout - time(NULL) : 
			  job->etimeout - time(NULL);

	return timeout;
}

/*
 *  Handle the two poll timeout cases:
 *    1. Job launch timed out
 *    2. Exit timeout has expired (either print a message or kill job)
 */
static void
_do_poll_timeout(job_t *job)
{
	time_t now = time(NULL);

	if ((job->ltimeout > 0) && (job->ltimeout <= now)) 
		_confirm_launch_complete(job);

	if ((job->etimeout > 0) && (job->etimeout <= now)) {
		if (!opt.max_wait)
			info("Warning: first task terminated %ds ago", 
			     opt.max_exit_timeout);
		else {
			error("First task exited %ds ago", opt.max_wait);
			report_task_status(job);
			update_job_state(job, SRUN_JOB_FAILED);
		}
		job->etimeout = 0;
	}
}

static void 
_msg_thr_poll(job_t *job)
{
	struct pollfd *fds;
	int i;

	fds = xmalloc(job->njfds * sizeof(*fds));

	_set_jfds_nonblocking(job);

	for (i = 0; i < job->njfds; i++)
		_poll_set_rd(fds[i], job->jfd[i]);

	while (!_job_msg_done(job)) {

		if (_do_poll(job, fds, _get_next_timeout(job)) == 0) {
			_do_poll_timeout(job);
			continue;
		}

		for (i = 0; i < job->njfds; i++) {
			unsigned short revents = fds[i].revents;
			if ((revents & POLLERR) || 
			    (revents & POLLHUP) ||
			    (revents & POLLNVAL))
				error("poll error on jfd %d: %m", fds[i].fd);
			else if (revents & POLLIN) 
				_accept_msg_connection(job, i);
		}
	}
	xfree(fds);	/* if we were to break out of while loop */
}

void *
msg_thr(void *arg)
{
	job_t *job = (job_t *) arg;

	debug3("msg thread pid = %ld", (long) getpid());

	slurm_uid = (uid_t) slurm_get_slurm_user_id();

	_msg_thr_poll(job);

	return (void *)1;
}

int 
msg_thr_create(job_t *job)
{
	int i;
	pthread_attr_t attr;

	for (i = 0; i < job->njfds; i++) {
		if ((job->jfd[i] = slurm_init_msg_engine_port(0)) < 0)
			fatal("init_msg_engine_port: %m");
		if (slurm_get_stream_addr(job->jfd[i], &job->jaddr[i]) < 0)
			fatal("slurm_get_stream_addr: %m");
		debug("initialized job control port %d\n",
		      ntohs(((struct sockaddr_in)job->jaddr[i]).sin_port));
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if ((errno = pthread_create(&job->jtid, &attr, &msg_thr, 
			            (void *)job)))
		fatal("Unable to start message thread: %m");

	debug("Started msg server thread (%d)", job->jtid);

	return SLURM_SUCCESS;
}
 
static void
_print_pid_list(const char *host, int ntasks, uint32_t *pid, 
		char *executable_name)
{
	if (_verbose) {
		int i;
		hostlist_t pids = hostlist_create(NULL);
		char buf[1024];

		for (i = 0; i < ntasks; i++) {
			snprintf(buf, sizeof(buf), "pids:%d", pid[i]);
			hostlist_push(pids, buf);
		}

		hostlist_ranged_string(pids, sizeof(buf), buf);
		verbose("%s: %s %s", host, executable_name, buf);
	}
}


