/*
 * dmfs-lv.c
 *
 * Copyright (C) 2001 Sistina Software
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* Heavily based upon ramfs */

#include <linux/config.h>
#include <linux/fs.h>

static int dmfs_lv_unlink(struct inode *dir, struct dentry *dentry)
{
	inode->i_nlink--;
	dput(dentry);
	return 0;
}

static struct dentry *dmfs_lv_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	char *name = dentry->d_name.name;

	switch(dentry->d_name.len) {
		case 5:
			if (memcmp("table", name, 5) == 0)
				inode = dmfs_create_table(dir, 0600);
			break;
		case 10:
			if (memcmp("suspend_IO", name, 10) == 0)
				inode = dmfs_create_suspend(dir, 0600);
			break;
	}

	d_add(dentry, inode);
	return NULL;
}

static int dmfs_lv_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	int i;
	struct dentry *dentry = filp->f_dentry;

	i = flip->f_pos;
	switch(i) {
		case 0:
			if (filldir(dirent, ".", 1, i, dentry->d_inode->i_ino, DT_DIR) < 0)
				break;
			i++;
			flip->f_pos++;
			/* fallthrough */
		case 1:
			if (filldir(dirent, "..", 2, i, dentry->d_parent->d_inode->i_ino, DT_DIR) < 0
				break;
			i++;
			filp->f_pos++;
			/* fallthrough */
		case 2:
			if (filldir(dirent, "table", 5, i, 2, DT_REG) < 0)
				break;
			i++;
			filp->f_pos++;
			/* fallthrough */
		case 3:
			if (filldir(dirent, "suspend_IO", 10, i, 3, DT_REG) < 0)
				break;
			i++;
			filp->f_pos++;
			/* fallthrough */
	}
	return 0;
}


static int dmfs_lv_sync(struct file *file, struct dentry *dentry, int datasync)
{
	return 0;
}

static struct dmfs_lv_file_operations = {
	read:		generic_read_dir,
	readdir:	dmfs_lv_readdir,
	fsync:		dmfs_lv_sync,
};

static struct dmfs_lv_inode_operations = {
	lookup:		dmfs_lv_lookup,
	unlink:		dmfs_lv_unlink,
};

struct inode *dmfs_create_lv(struct inode *dir, struct dentry *dentry, int mode)
{
	struct inode *inode = new_inode(sb);

	if (inode) {
		inode->i_mode = mode | S_IFDIR;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_rdev = NODEV;
		inode->i_atime = inode->i_ctime = inode->i_mtime = CURRENT_TIME;
		inode->i_fop = &dmfs_lv_file_operations;
		inode->i_op = &dmfs_lv_dir_operations;
	}

	return inode;
}


