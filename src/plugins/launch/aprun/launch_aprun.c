/*****************************************************************************\
 *  launch_aprun.c - Define job launch using Cray's aprun.
 *
 *****************************************************************************
 *  Copyright (C) 2012 SchedMD LLC
 *  Written by Danny Auble <da@schedmd.com>
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <ctype.h>

#include "src/common/slurm_xlator.h"
#include "src/common/parse_time.h"
#include "src/srun/libsrun/launch.h"
#include "src/srun/libsrun/multi_prog.h"

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  SLURM uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *      <application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "task" for task control) and <method> is a description
 * of how this plugin satisfies that application.  SLURM will only load
 * a task plugin if the plugin_type string has a prefix of "task/".
 *
 * plugin_version - an unsigned 32-bit integer giving the version number
 * of the plugin.  If major and minor revisions are desired, the major
 * version number may be multiplied by a suitable magnitude constant such
 * as 100 or 1000.  Various SLURM versions will likely require a certain
 * minimum version for their plugins as this API matures.
 */
const char plugin_name[]        = "launch aprun plugin";
const char plugin_type[]        = "launch/aprun";
const uint32_t plugin_version   = 101;

static pid_t aprun_pid = 0;

/* Convert a SLURM hostlist expression into the equivalent node index
 * value expression.
 */
static char *_get_nids(char *nodelist)
{
	char *nids;
	int i = 0, i2 = 0;

	if (!nodelist)
		return NULL;
//	info("got %s", nodelist);
	nids = xmalloc(sizeof(char) * strlen(nodelist));
	while (nodelist[i] && !isdigit(nodelist[i]))
		i++;

	while (nodelist[i] && nodelist[i] != ']')
		nids[i2++] = nodelist[i++];

//	info("returning %s", nids);
	return nids;
}

/*
 * Parse a multi-prog input file line
 * line IN - line to parse
 * command_pos IN/OUT - where in opt.argv we are
 * count IN - which command we are on
 * return 0 if empty line, 1 if added
 */
static int _parse_prog_line(char *in_line, int *command_pos, int count)
{
	int i, cmd_inx;
	int first_task_inx, last_task_inx;
	hostset_t hs = NULL;
	char *tmp_str = NULL;

	xassert(opt.ntasks);

	/* Get the task ID string */
	for (i = 0; in_line[i]; i++)
		if (!isspace(in_line[i]))
			break;

	if (!in_line[i]) /* empty line */
		return 0;
	else if (in_line[i] == '#')
		return 0;
	else if (!isdigit(in_line[i]))
		goto bad_line;
	first_task_inx = i;
	for (i++; in_line[i]; i++) {
		if (isspace(in_line[i]))
			break;
	}
	if (!isspace(in_line[i]))
		goto bad_line;
	last_task_inx = i;


	/* Now transfer data to the function arguments */
	in_line[last_task_inx] = '\0';
	xstrfmtcat(tmp_str, "[%s]", in_line + first_task_inx);
	hs = hostset_create(tmp_str);
	xfree(tmp_str);
	in_line[last_task_inx] = ' ';
	if (!hs)
		goto bad_line;


	if (count) {
		opt.argc += 1;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[(*command_pos)++] = xstrdup(":");
	}
	opt.argc += 2;
	xrealloc(opt.argv, opt.argc * sizeof(char *));
	opt.argv[(*command_pos)++] = xstrdup("-n");
	opt.argv[(*command_pos)++] = xstrdup_printf("%d", hostset_count(hs));
	hostset_destroy(hs);

	/* Get the command */
	for (i++; in_line[i]; i++) {
		if (!isspace(in_line[i]))
			break;
	}
	if (in_line[i] == '\0')
		goto bad_line;

	cmd_inx = i;
	for ( ; in_line[i]; i++) {
		if (isspace(in_line[i])) {
			if (i > cmd_inx) {
				int diff = i - cmd_inx + 1;
				char tmp_char[diff + 1];
				snprintf(tmp_char, diff, "%s",
					 in_line + cmd_inx);
				opt.argc += 1;
				xrealloc(opt.argv, opt.argc * sizeof(char *));
				opt.argv[(*command_pos)++] = xstrdup(tmp_char);
			}
			cmd_inx = i + 1;
		} else if (in_line[i] == '\n')
			break;
	}

	return 1;

bad_line:
	error("invalid input line: %s", in_line);

	return 0;
}

static void _handle_multi_prog(char *in_file, int *command_pos)
{
	char in_line[512];
	FILE *fp;
	int count = 0;

	if (verify_multi_name(in_file, opt.ntasks))
		exit(error_exit);

	fp = fopen(in_file, "r");
	if (!fp) {
		fatal("fopen(%s): %m", in_file);
		return;
	}

	while (fgets(in_line, sizeof(in_line), fp)) {
		if (_parse_prog_line(in_line, command_pos, count))
			count++;
	}

	return;
}

static void _unblock_signals(void)
{
	sigset_t set;
	int i;

	for (i = 0; sig_array[i]; i++) {
		/* eliminate pending signals, then set to default */
		xsignal(sig_array[i], SIG_IGN);
		xsignal(sig_array[i], SIG_DFL);
	}
	sigemptyset(&set);
	xsignal_set_mask (&set);
}

/*
 * init() is called when the plugin is loaded, before any other functions
 *	are called.  Put global initialization here.
 */
extern int init(void)
{
	verbose("%s loaded", plugin_name);
	return SLURM_SUCCESS;
}

/*
 * fini() is called when the plugin is removed. Clear any allocated
 *	storage here.
 */
extern int fini(void)
{
	return SLURM_SUCCESS;
}

extern int launch_p_setup_srun_opt(char **rest)
{
	int command_pos = 0;

	if (opt.test_only) {
		error("--test-only not supported with aprun");
		exit (1);
	} else if (opt.no_alloc) {
		error("--no-allocate not supported with aprun");
		exit (1);
	}
	if (opt.slurmd_debug != LOG_LEVEL_QUIET) {
		error("--slurmd-debug not supported with aprun");
		opt.slurmd_debug = LOG_LEVEL_QUIET;
	}

	opt.argc += 2;

	opt.argv = (char **) xmalloc(opt.argc * sizeof(char *));

	opt.argv[command_pos++] = xstrdup("aprun");
	/* Set default job name to the executable name rather than
	 * "aprun" */
	if (!opt.job_name_set_cmd && (1 < opt.argc)) {
		opt.job_name_set_cmd = true;
		opt.job_name = xstrdup(rest[0]);
	}

	if (opt.cpus_per_task) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-d");
		opt.argv[command_pos++] = xstrdup_printf(
			"%u", opt.cpus_per_task);
	}

	if (opt.shared != (uint16_t)NO_VAL) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-F");
		opt.argv[command_pos++] = xstrdup("share");
	} else if (opt.exclusive) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-F");
		opt.argv[command_pos++] = xstrdup("exclusive");
	}

	if (opt.nodelist) {
		char *nids = _get_nids(opt.nodelist);
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-L");
		opt.argv[command_pos++] = xstrdup(nids);
		xfree(nids);
	}

	if (opt.mem_per_cpu != NO_VAL) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-m");
		opt.argv[command_pos++] = xstrdup_printf("%u", opt.mem_per_cpu);
	}

	if (opt.ntasks_per_node != NO_VAL) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-N");
		opt.argv[command_pos++] = xstrdup_printf(
			"%u", opt.ntasks_per_node);
		if (!opt.ntasks && opt.min_nodes)
			opt.ntasks = opt.ntasks_per_node *  opt.min_nodes;
	} else if (opt.min_nodes) {
		uint32_t tasks_per_node;
		if (!opt.ntasks)
			opt.ntasks = opt.min_nodes;
		tasks_per_node = (opt.ntasks + opt.min_nodes - 1) /
			opt.min_nodes;
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-N");
		opt.argv[command_pos++] = xstrdup_printf("%u", tasks_per_node);
	}

	if (opt.ntasks && !opt.multi_prog) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-n");
		opt.argv[command_pos++] = xstrdup_printf("%u", opt.ntasks);
	}

	if (opt.quiet) {
		opt.argc += 1;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-q");
	}

	if (opt.ntasks_per_socket != NO_VAL) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-S");
		opt.argv[command_pos++] = xstrdup_printf(
			"%u", opt.ntasks_per_socket);
	}

	if (opt.sockets_per_node != NO_VAL) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-sn");
		opt.argv[command_pos++] = xstrdup_printf(
			"%u", opt.sockets_per_node);
	}

	if (opt.mem_bind && strstr(opt.mem_bind, "local")) {
		opt.argc += 1;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-ss");
	}

	if (opt.time_limit_str) {
		opt.argc += 2;
		xrealloc(opt.argv, opt.argc * sizeof(char *));
		opt.argv[command_pos++] = xstrdup("-t");
		opt.argv[command_pos++] = xstrdup_printf(
			"%d", time_str2secs(opt.time_limit_str));
	}

	if (opt.launcher_opts) {
		char *save_ptr = NULL, *tok;
		char *tmp = xstrdup(opt.launcher_opts);
		tok = strtok_r(tmp, " ", &save_ptr);
		while (tok) {
			opt.argc++;
			xrealloc(opt.argv, opt.argc * sizeof(char *));
			opt.argv[command_pos++]  = xstrdup(tok);
			tok = strtok_r(NULL, " ", &save_ptr);
		}
		xfree(tmp);
	}


	/* These are srun options that are not supported by aprun, but
	   here just in case in the future they add them.

	   if (opt.disable_status) {
	   xstrcat(cmd_line, " --disable-status");
	   }

	   if (opt.epilog) {
	   xstrfmtcat(cmd_line, " --epilog=", opt.epilog);
	   }

	   if (kill_on_bad_exit) {
	   xstrcat(cmd_line, " --kill-on-bad-exit");
	   }

	   if (label) {
	   xstrcat(cmd_line, " --label");
	   }

	   if (opt.mpi_type) {
	   xstrfmtcat(cmd_line, " --mpi=", opt.mpi_type);
	   }

	   if (opt.msg_timeout) {
	   xstrfmtcat(cmd_line, " --msg-timeout=", opt.msg_timeout);
	   }

	   if (no_allocate) {
	   xstrcat(cmd_line, " --no-allocate");
	   }

	   if (opt.open_mode) {
	   xstrcat(cmd_line, " --open-mode=", opt.open_mode);
	   }

	   if (preserve_env) {
	   xstrcat(cmd_line, " --preserve_env");
	   }


	   if (opt.prolog) {
	   xstrcat(cmd_line, " --prolog=", opt.prolog );
	   }


	   if (opt.propagate) {
	   xstrcat(cmd_line, " --propagate", opt.propagate );
	   }

	   if (pty) {
	   xstrcat(cmd_line, " --pty");
	   }

	   if (quit_on_interrupt) {
	   xstrcat(cmd_line, " --quit-on-interrupt");
	   }


	   if (opt.relative) {
	   xstrfmtcat(cmd_line, " --relative=", opt.relative);
	   }

	   if (restart_dir) {
	   xstrfmtcat(cmd_line, " --restart-dir=", opt.restart_dir);
	   }


	   if (resv_port) {
	   xstrcat(cmd_line, "--resv-port");
	   }

	   if (opt.slurm_debug) {
	   xstrfmtcat(cmd_line, " --slurmd-debug=", opt.slurm_debug);
	   }

	   if (opttask_epilog) {
	   xstrfmtcat(cmd_line, " --task-epilog=", opt.task_epilog);
	   }

	   if (opt.task_prolog) {
	   xstrfmtcat(cmd_line, " --task-prolog", opt.task_prolog);
	   }

	   if (test_only) {
	   xstrcat(cmd_line, " --test-only");
	   }

	   if (unbuffered) {
	   xstrcat(cmd_line, " --unbuffered");
	   }

	*/

	if (opt.multi_prog) {
		_handle_multi_prog(rest[0], &command_pos);
		/* just so we don't tack on the script to the aprun line */
		command_pos = opt.argc;
	}

	return command_pos;
}

extern int launch_p_handle_multi_prog_verify(int command_pos)
{
	if (opt.multi_prog)
		return 1;
	return 0;
}

extern int launch_p_create_job_step(srun_job_t *job, bool use_all_cpus,
				    void (*signal_function)(int),
				    sig_atomic_t *destroy_job)
{
	if (opt.launch_cmd) {
		int i = 0;
		char *cmd_line = NULL;

		while (opt.argv[i])
			xstrfmtcat(cmd_line, "%s ", opt.argv[i++]);
		printf("%s\n", cmd_line);
		xfree(cmd_line);
		exit(0);
	}
	return 0;
}

extern int launch_p_step_launch(
	srun_job_t *job, slurm_step_io_fds_t *cio_fds, uint32_t *global_rc,
	void (*signal_function)(int))
{
	int rc = 0;

	aprun_pid = fork();
	if (aprun_pid < 0) {
		error("fork: %m");
		return 1;
	} else if (aprun_pid > 0) {
		if (waitpid(aprun_pid, &rc, 0) < 0)
			error("Unable to reap aprun child process");
		*global_rc = rc;
		/* Just because waitpid returns something doesn't mean
		   this function failed so always set it back to 0.
		*/
		rc = 0;
	} else {
		setpgrp();
		_unblock_signals();
		/* dup stdio onto our open fds */
		if ((dup2(cio_fds->in.fd, 0) == -1) ||
		    (dup2(cio_fds->out.fd, 1) == -1) ||
		    (dup2(cio_fds->err.fd, 2) == -1)) {
			error("dup2: %m");
			return 1;
		}
		execvp(opt.argv[0], opt.argv);
		error("execv(aprun) error: %m");
		return 1;
	}

	return rc;
}

extern int launch_p_step_wait(srun_job_t *job, bool got_alloc)
{
	return SLURM_SUCCESS;
}

extern int launch_p_step_terminate(void)
{
	return SLURM_SUCCESS;
}


extern void launch_p_print_status(void)
{

}

extern void launch_p_fwd_signal(int signal)
{
	if (aprun_pid)
		kill(aprun_pid, signal);
}
