/*
 * dm-table.c
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

/*
 * Changelog
 *
 *     16/08/2001 - First version [Joe Thornber]
 */

#include "dm.h"

/* ceiling(n / size) * size */
static inline ulong round_up(ulong n, ulong size)
{
	ulong r = n % size;
	return n + (r ? (size - r) : 0);
}

/* ceiling(n / size) */
static inline ulong div_up(ulong n, ulong size)
{
	return round_up(n, size) / size;
}

/* ceiling(log_size(n)) */
static uint int_log(ulong base, ulong n)
{
	int result = 0;

	while (n != 1) {
		n = div_up(n, base);
		result++;
	}

	return result;
}

/*
 * return the highest key that you could lookup
 * from the n'th node on level l of the btree.
 */
static offset_t high(struct mapped_device *md, int l, int n)
{
	while (1) {
		if (n >= md->counts[l])
			return (offset_t) -1;

		if (l == md->depth - 1)
			return md->index[l][((n + 1) * KEYS_PER_NODE) - 1];

		l++;
		n = (n + 1) * (KEYS_PER_NODE + 1) - 1;
	}

	return -1;
}

/*
 * fills in a level of the btree based on the
 * highs of the level below it.
 */
static int setup_btree_index(int l, struct mapped_device *md)
{
	int n, c, cn;

	for (n = 0, cn = 0; n < md->counts[l]; n++) {
		offset_t *k = md->index[l] + (n * KEYS_PER_NODE);

		for (c = 0; c < KEYS_PER_NODE; c++)
			k[c] = high(md, l + 1, cn++);
		cn++;		/* one extra for the child that's
                                   greater than all keys */
	}

	return 0;
}

void dm_table_free(struct mapped_device *md)
{
	int i;
	for (i = 0; i < md->depth; i++) {
		vfree(md->index[i]);
		md->index[i] = 0;
	}

	vfree(md->targets);

	md->highs = 0;
	md->targets = 0;

	md->num_targets = 0;
	md->num_allocated = 0;
}

/*
 * md->highs, and md->targets are managed as
 * dynamic arrays during a table load.
 */
static int alloc_targets(struct mapped_device *md, int num)
{
	offset_t *n_highs;
	struct target *n_targets;

	if (!(n_highs = vmalloc(sizeof(*n_highs) * num)))
		return -ENOMEM;

	if (!(n_targets = vmalloc(sizeof(*n_targets) * num))) {
		vfree(n_highs);
		return -ENOMEM;
	}

	if (md->num_targets) {
		memcpy(n_highs, md->highs,
		       sizeof(*n_highs) * md->num_targets);

		memcpy(n_targets, md->targets,
		       sizeof(*n_targets) * md->num_targets);
	}

	vfree(md->highs);
	vfree(md->targets);

	md->num_allocated = num;
	md->highs = n_highs;
	md->targets = n_targets;

	return 0;
}

int dm_table_start(struct mapped_device *md)
{
	int r;
	set_bit(DM_LOADING, &md->state);

	dm_table_free(md);

	/* allocate a single nodes worth to start with */
	if ((r = alloc_targets(md, KEYS_PER_NODE)))
		return r;

	return 0;
}

static inline int check_space(struct mapped_device *md)
{
	if (md->num_targets >= md->num_allocated)
		return alloc_targets(md, md->num_allocated * 2);

	return 0;
}

int dm_table_add_entry(struct mapped_device *md, offset_t high,
		       dm_map_fn target, void *context)
{
	int r;

	if ((r = check_space(md)))
		return r;

	md->highs[md->num_targets] = high;
	md->targets[md->num_targets].map = target;
	md->targets[md->num_targets].private = context;
	md->num_targets++;

	return 0;
}

int dm_table_complete(struct mapped_device *md)
{
	int i, leaf_nodes;

	clear_bit(DM_LOADING, &md->state);

	/* how many indexes will the btree have ? */
	leaf_nodes = div_up(md->num_targets, KEYS_PER_NODE);
	i = 1 + int_log(leaf_nodes, KEYS_PER_NODE + 1);

	md->depth = i;
	md->counts[md->depth - 1] = div_up(md->num_targets, KEYS_PER_NODE);

	while (--i)
		md->counts[i - 1] = div_up(md->counts[i], KEYS_PER_NODE + 1);

	for (i = 0; i < (md->depth - 1); i++) {
		size_t s = NODE_SIZE * md->counts[i];
		md->index[i] = vmalloc(s);
		memset(md->index[i], -1, s);
	}

	/* bottom layer is easy */
	md->index[md->depth - 1] = md->highs;

	/* fill in higher levels */
	for (i = md->depth - 1; i; i--)
		setup_btree_index(i - 1, md);

	set_bit(DM_LOADED, &md->state);
	return 0;
}


