/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.  
 * Copyright (C) 2004 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

static int vg_backup_single(struct cmd_context *cmd, const char *vg_name,
			    struct volume_group *vg, int consistent,
			    void *handle)
{
	if (!vg) {
		log_error("Volume group \"%s\" not found", vg_name);
		return ECMD_FAILED;
	}

	if (!consistent)
		log_error("Warning: Volume group \"%s\" inconsistent", vg_name);

	if (arg_count(cmd, file_ARG)) {
		backup_to_file(arg_value(cmd, file_ARG), vg->cmd->cmd_line, vg);

	} else {
		if (!consistent) {
			log_error("No backup taken: specify filename with -f "
				  "to backup an inconsistent VG");
			stack;
			return ECMD_FAILED;
		}

		/* just use the normal backup code */
		backup_enable(1);	/* force a backup */
		if (!backup(vg)) {
			stack;
			return ECMD_FAILED;
		}
	}

	log_print("Volume group \"%s\" successfully backed up.", vg_name);
	return ECMD_PROCESSED;
}

int vgcfgbackup(struct cmd_context *cmd, int argc, char **argv)
{
	int ret;

	if (partial_mode())
		init_pvmove(1);

	ret = process_each_vg(cmd, argc, argv, LCK_VG_READ, 0, NULL,
			      &vg_backup_single);

	init_pvmove(0);

	return ret;
}
