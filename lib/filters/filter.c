/*
 * Copyright (C) 2001 Sistina Software
 *
 * lvm is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * lvm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "lib.h"
#include "dev-cache.h"
#include "filter.h"
#include "lvm-string.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <linux/kdev_t.h>

#define NUMBER_OF_MAJORS 256

typedef struct {
	char *name;
	int max_partitions;
} device_info_t;

static int _md_major = -1;

/* FIXME Move list into config file */
static device_info_t device_info[] = {
	{"ide", 16},		/* IDE disk */
	{"sd", 16},		/* SCSI disk */
	{"md", 16},		/* Multiple Disk driver (SoftRAID) */
	{"loop", 16},		/* Loop device */
	{"dasd", 4},		/* DASD disk (IBM S/390, zSeries) */
	{"dac960", 8},		/* DAC960 */
	{"nbd", 16},		/* Network Block Device */
	{"ida", 16},		/* Compaq SMART2 */
	{"cciss", 16},		/* Compaq CCISS array */
	{"ubd", 16},		/* User-mode virtual block device */
	{"ataraid", 16},	/* ATA Raid */
	{NULL, 0}
};

static int *scan_proc_dev(const char *proc);

static int passes_lvm_type_device_filter(struct dev_filter *f,
					 struct device *dev)
{
	int fd;
	const char *name = dev_name(dev);

	/* Is this a recognised device type? */
	if (!(((int *) f->private)[MAJOR(dev->dev)]))
		return 0;

	/* Check it's accessible */
	if ((fd = open(name, O_RDONLY)) < 0) {
		log_debug("Unable to open %s: %s", name, strerror(errno));
		return 0;
	}

	close(fd);

	return 1;
}

struct dev_filter *lvm_type_filter_create(const char *proc)
{
	struct dev_filter *f;

	if (!(f = dbg_malloc(sizeof(struct dev_filter)))) {
		log_error("LVM type filter allocation failed");
		return NULL;
	}

	f->passes_filter = passes_lvm_type_device_filter;
	f->destroy = lvm_type_filter_destroy;

	if (!(f->private = scan_proc_dev(proc)))
		return NULL;

	return f;
}

int md_major(void)
{
	return _md_major;
}

void lvm_type_filter_destroy(struct dev_filter *f)
{
	dbg_free(f->private);
	dbg_free(f);
	return;
}

static int *scan_proc_dev(const char *proc)
{
	char line[80];
	char proc_devices[PATH_MAX];
	FILE *pd = NULL;
	int ret = 0;
	int i, j = 0;
	int line_maj = 0;
	int blocksection = 0;
	int dev_len = 0;

	int *max_partitions_by_major;

	if (!(max_partitions_by_major =
	      dbg_malloc(sizeof(int) * NUMBER_OF_MAJORS))) {
		log_error("Filter failed to allocate max_partitions_by_major");
		return NULL;
	}

	if (lvm_snprintf(proc_devices, sizeof(proc_devices),
			 "%s/devices", proc) < 0) {
		log_error("Failed to create /proc/devices string");
		return NULL;
	}

	if (!(pd = fopen(proc_devices, "r"))) {
		log_sys_error("fopen", proc_devices);
		return NULL;
	}

	memset(max_partitions_by_major, 0, sizeof(int) * NUMBER_OF_MAJORS);
	while (fgets(line, 80, pd) != NULL) {
		i = 0;
		while (line[i] == ' ' && line[i] != '\0')
			i++;

		/* If it's not a number it may be name of section */
		line_maj = atoi(((char *) (line + i)));
		if (!line_maj) {
			blocksection = (line[i] == 'B') ? 1 : 0;
			continue;
		}

		/* We only want block devices ... */
		if (!blocksection)
			continue;

		/* Find the start of the device major name */
		while (line[i] != ' ' && line[i] != '\0')
			i++;
		while (line[i] == ' ' && line[i] != '\0')
			i++;

		/* Look for md device */
		if (!strncmp("md", line + i, 2) && isspace(*(line + i + 2)))
			_md_major = line_maj;

		/* Go through the valid device names and if there is a
		   match store max number of partitions */
		for (j = 0; device_info[j].name != NULL; j++) {

			dev_len = strlen(device_info[j].name);
			if (dev_len <= strlen(line + i)
			    && !strncmp(device_info[j].name, line + i, dev_len)
			    && (line_maj < NUMBER_OF_MAJORS)) {
				max_partitions_by_major[line_maj] =
				    device_info[j].max_partitions;
				ret++;
				break;
			}
		}
	}
	fclose(pd);
	return max_partitions_by_major;
}
