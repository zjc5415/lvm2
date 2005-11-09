/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2005 Red Hat, Inc. All rights reserved.
 *
 * This file is part of the device-mapper userspace tools.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LIB_DEVICE_MAPPER_H
#define LIB_DEVICE_MAPPER_H

#include <inttypes.h>
#include <sys/types.h>

#ifdef linux
#  include <linux/types.h>
#endif

#include <limits.h>
#include <string.h>
#include <stdlib.h>

/*****************************************************************
 * The first section of this file provides direct access to the 
 * individual device-mapper ioctls.
 ****************************************************************/

/*
 * Since it is quite laborious to build the ioctl
 * arguments for the device-mapper people are
 * encouraged to use this library.
 *
 * You will need to build a struct dm_task for
 * each ioctl command you want to execute.
 */

typedef void (*dm_log_fn) (int level, const char *file, int line,
			   const char *f, ...);

/*
 * The library user may wish to register their own
 * logging function, by default errors go to stderr.
 * Use dm_log_init(NULL) to restore the default log fn.
 */
void dm_log_init(dm_log_fn fn);
void dm_log_init_verbose(int level);

enum {
	DM_DEVICE_CREATE,
	DM_DEVICE_RELOAD,
	DM_DEVICE_REMOVE,
	DM_DEVICE_REMOVE_ALL,

	DM_DEVICE_SUSPEND,
	DM_DEVICE_RESUME,

	DM_DEVICE_INFO,
	DM_DEVICE_DEPS,
	DM_DEVICE_RENAME,

	DM_DEVICE_VERSION,

	DM_DEVICE_STATUS,
	DM_DEVICE_TABLE,
	DM_DEVICE_WAITEVENT,

	DM_DEVICE_LIST,

	DM_DEVICE_CLEAR,

	DM_DEVICE_MKNODES,

	DM_DEVICE_LIST_VERSIONS,
	
	DM_DEVICE_TARGET_MSG
};

struct dm_task;

struct dm_task *dm_task_create(int type);
void dm_task_destroy(struct dm_task *dmt);

int dm_task_set_name(struct dm_task *dmt, const char *name);
int dm_task_set_uuid(struct dm_task *dmt, const char *uuid);

/*
 * Retrieve attributes after an info.
 */
struct dm_info {
	int exists;
	int suspended;
	int live_table;
	int inactive_table;
	int32_t open_count;
	uint32_t event_nr;
	uint32_t major;
	uint32_t minor;		/* minor device number */
	int read_only;		/* 0:read-write; 1:read-only */

	int32_t target_count;
};

struct dm_deps {
	uint32_t count;
	uint32_t filler;
	uint64_t device[0];
};

struct dm_names {
	uint64_t dev;
	uint32_t next;		/* Offset to next struct from start of this struct */
	char name[0];
};

struct dm_versions {
        uint32_t next;		/* Offset to next struct from start of this struct */
        uint32_t version[3];

        char name[0];
};

int dm_get_library_version(char *version, size_t size);
int dm_task_get_driver_version(struct dm_task *dmt, char *version, size_t size);
int dm_task_get_info(struct dm_task *dmt, struct dm_info *dmi);
const char *dm_task_get_name(struct dm_task *dmt);
const char *dm_task_get_uuid(struct dm_task *dmt);

struct dm_deps *dm_task_get_deps(struct dm_task *dmt);
struct dm_names *dm_task_get_names(struct dm_task *dmt);
struct dm_versions *dm_task_get_versions(struct dm_task *dmt);

int dm_task_set_ro(struct dm_task *dmt);
int dm_task_set_newname(struct dm_task *dmt, const char *newname);
int dm_task_set_minor(struct dm_task *dmt, int minor);
int dm_task_set_major(struct dm_task *dmt, int major);
int dm_task_set_event_nr(struct dm_task *dmt, uint32_t event_nr);
int dm_task_set_message(struct dm_task *dmt, const char *message);
int dm_task_set_sector(struct dm_task *dmt, uint64_t sector);
int dm_task_no_open_count(struct dm_task *dmt);
int dm_task_skip_lockfs(struct dm_task *dmt);

/*
 * Use these to prepare for a create or reload.
 */
int dm_task_add_target(struct dm_task *dmt,
		       uint64_t start,
		       uint64_t size, const char *ttype, const char *params);

/*
 * Format major/minor numbers correctly for input to driver
 */
int dm_format_dev(char *buf, int bufsize, uint32_t dev_major, uint32_t dev_minor);

/* Use this to retrive target information returned from a STATUS call */
void *dm_get_next_target(struct dm_task *dmt,
			 void *next, uint64_t *start, uint64_t *length,
			 char **target_type, char **params);

/*
 * Call this to actually run the ioctl.
 */
int dm_task_run(struct dm_task *dmt);

/*
 * Configure the device-mapper directory
 */
int dm_set_dev_dir(const char *dir);
const char *dm_dir(void);

/*
 * Determine whether a major number belongs to device-mapper or not.
 */
int dm_is_dm_major(uint32_t major);

/*
 * Release library resources
 */
void dm_lib_release(void);
void dm_lib_exit(void) __attribute((destructor));

/*
 * Use NULL for all devices.
 */
int dm_mknodes(const char *name);
int dm_driver_version(char *version, size_t size);

/******************************************************
 * Functions to build and manipulate trees of devices *
 ******************************************************/
struct dm_tree;
struct dm_tree_node;

/*
 * Initialise an empty dependency tree.
 *
 * The tree consists of a root node together with one node for each mapped 
 * device which has child nodes for each device referenced in its table.
 *
 * Every node in the tree has one or more children and one or more parents.
 *
 * The root node is the parent/child of every node that doesn't have other 
 * parents/children.
 */
struct dm_tree *dm_tree_create(void);
void dm_tree_free(struct dm_tree *tree);

/*
 * Add nodes to the tree for a given device and all the devices it uses.
 */
int dm_tree_add_dev(struct dm_tree *tree, uint32_t major, uint32_t minor);

/*
 * Add a new node to the tree if it doesn't already exist.
 */
struct dm_tree_node *dm_tree_add_new_dev(struct dm_tree *tree,
                                            const char *name,
                                            const char *uuid,
                                            uint32_t major, uint32_t minor,
                                            int read_only,
                                            int clear_inactive,
                                            void *context);

/*
 * Search for a node in the tree.
 * Set major and minor to 0 or uuid to NULL to get the root node.
 */
struct dm_tree_node *dm_tree_find_node(struct dm_tree *tree,
					  uint32_t major,
					  uint32_t minor);
struct dm_tree_node *dm_tree_find_node_by_uuid(struct dm_tree *tree,
						  const char *uuid);

/*
 * Use this to walk through all children of a given node.
 * Set handle to NULL in first call.
 * Returns NULL after the last child.
 * Set inverted to use inverted tree.
 */
struct dm_tree_node *dm_tree_next_child(void **handle,
					   struct dm_tree_node *parent,
					   uint32_t inverted);

/*
 * Get properties of a node.
 */
const char *dm_tree_node_get_name(struct dm_tree_node *node);
const char *dm_tree_node_get_uuid(struct dm_tree_node *node);
const struct dm_info *dm_tree_node_get_info(struct dm_tree_node *node);
void *dm_tree_node_get_context(struct dm_tree_node *node);

/*
 * Returns the number of children of the given node (excluding the root node).
 * Set inverted for the number of parents.
 */
int dm_tree_node_num_children(struct dm_tree_node *node, uint32_t inverted);

/*
 * Deactivate a device plus all dependencies.
 * Ignores devices that don't have a uuid starting with uuid_prefix.
 */
int dm_tree_deactivate_children(struct dm_tree_node *dnode,
				   const char *uuid_prefix,
				   size_t uuid_prefix_len);
/*
 * Preload/create a device plus all dependencies.
 * Ignores devices that don't have a uuid starting with uuid_prefix.
 */
int dm_tree_preload_children(struct dm_tree_node *dnode,
                                 const char *uuid_prefix,
                                 size_t uuid_prefix_len,
				 int resume_children);

/*
 * Resume a device plus all dependencies.
 * Ignores devices that don't have a uuid starting with uuid_prefix.
 */
int dm_tree_activate_children(struct dm_tree_node *dnode,
                                 const char *uuid_prefix,
                                 size_t uuid_prefix_len);

/*
 * Suspend a device plus all dependencies.
 * Ignores devices that don't have a uuid starting with uuid_prefix.
 */
int dm_tree_suspend_children(struct dm_tree_node *dnode,
				   const char *uuid_prefix,
				   size_t uuid_prefix_len);

/*
 * Is the uuid prefix present in the tree?
 * Only returns 0 if every node was checked successfully.
 * Returns 1 if the tree walk has to be aborted.
 */
int dm_tree_children_use_uuid(struct dm_tree_node *dnode,
				 const char *uuid_prefix,
				 size_t uuid_prefix_len);

/*
 * Construct tables for new nodes before activating them.
 */
int dm_tree_node_add_snapshot_origin_target(struct dm_tree_node *dnode,
					       uint64_t size,
					       const char *origin_uuid);
int dm_tree_node_add_snapshot_target(struct dm_tree_node *node,
					uint64_t size,
					const char *origin_uuid,
					const char *cow_uuid,
					int persistent,
					uint32_t chunk_size);
int dm_tree_node_add_error_target(struct dm_tree_node *node,
				     uint64_t size);
int dm_tree_node_add_zero_target(struct dm_tree_node *node,
				    uint64_t size);
int dm_tree_node_add_linear_target(struct dm_tree_node *node,
				      uint64_t size);
int dm_tree_node_add_striped_target(struct dm_tree_node *node,
				       uint64_t size,
				       uint32_t stripe_size);
int dm_tree_node_add_mirror_target(struct dm_tree_node *node,
				      uint64_t size);
int dm_tree_node_add_mirror_target_log(struct dm_tree_node *node,
					  uint32_t region_size,
					  unsigned clustered,
					  const char *log_uuid,
					  unsigned area_count);
int dm_tree_node_add_target_area(struct dm_tree_node *node,
				    const char *dev_name,
				    const char *dlid,
				    uint64_t offset);

/*****************************************************************************
 * Library functions
 *****************************************************************************/

/*******************
 * Memory management
 *******************/

void *dm_malloc_aux(size_t s, const char *file, int line);
#define dm_malloc(s) dm_malloc_aux((s), __FILE__, __LINE__)

char *dm_strdup(const char *str);

#ifdef DEBUG_MEM

void dm_free_aux(void *p);
void *dm_realloc_aux(void *p, unsigned int s, const char *file, int line);
int dm_dump_memory(void);
void dm_bounds_check(void);

#  define dm_free(p) dm_free_aux(p)
#  define dm_realloc(p, s) dm_realloc_aux(p, s, __FILE__, __LINE__)

#else

#  define dm_free(p) free(p)
#  define dm_realloc(p, s) realloc(p, s)
#  define dm_dump_memory()
#  define dm_bounds_check()

#endif

/*
 * The pool allocator is useful when you are going to allocate
 * lots of memory, use the memory for a bit, and then free the
 * memory in one go.  A surprising amount of code has this usage
 * profile.
 *
 * You should think of the pool as an infinite, contiguous chunk
 * of memory.  The front of this chunk of memory contains
 * allocated objects, the second half is free.  dm_pool_alloc grabs
 * the next 'size' bytes from the free half, in effect moving it
 * into the allocated half.  This operation is very efficient.
 *
 * dm_pool_free frees the allocated object *and* all objects
 * allocated after it.  It is important to note this semantic
 * difference from malloc/free.  This is also extremely
 * efficient, since a single dm_pool_free can dispose of a large
 * complex object.
 *
 * dm_pool_destroy frees all allocated memory.
 *
 * eg, If you are building a binary tree in your program, and
 * know that you are only ever going to insert into your tree,
 * and not delete (eg, maintaining a symbol table for a
 * compiler).  You can create yourself a pool, allocate the nodes
 * from it, and when the tree becomes redundant call dm_pool_destroy
 * (no nasty iterating through the tree to free nodes).
 *
 * eg, On the other hand if you wanted to repeatedly insert and
 * remove objects into the tree, you would be better off
 * allocating the nodes from a free list; you cannot free a
 * single arbitrary node with pool.
 */

struct dm_pool;

/* constructor and destructor */
struct dm_pool *dm_pool_create(const char *name, size_t chunk_hint);
void dm_pool_destroy(struct dm_pool *p);

/* simple allocation/free routines */
void *dm_pool_alloc(struct dm_pool *p, size_t s);
void *dm_pool_alloc_aligned(struct dm_pool *p, size_t s, unsigned alignment);
void dm_pool_empty(struct dm_pool *p);
void dm_pool_free(struct dm_pool *p, void *ptr);

/*
 * Object building routines:
 *
 * These allow you to 'grow' an object, useful for
 * building strings, or filling in dynamic
 * arrays.
 *
 * It's probably best explained with an example:
 *
 * char *build_string(struct dm_pool *mem)
 * {
 *      int i;
 *      char buffer[16];
 *
 *      if (!dm_pool_begin_object(mem, 128))
 *              return NULL;
 *
 *      for (i = 0; i < 50; i++) {
 *              snprintf(buffer, sizeof(buffer), "%d, ", i);
 *              if (!dm_pool_grow_object(mem, buffer, strlen(buffer)))
 *                      goto bad;
 *      }
 *
 *	// add null
 *      if (!dm_pool_grow_object(mem, "\0", 1))
 *              goto bad;
 *
 *      return dm_pool_end_object(mem);
 *
 * bad:
 *
 *      dm_pool_abandon_object(mem);
 *      return NULL;
 *}
 *
 * So start an object by calling dm_pool_begin_object
 * with a guess at the final object size - if in
 * doubt make the guess too small.
 *
 * Then append chunks of data to your object with
 * dm_pool_grow_object.  Finally get your object with
 * a call to dm_pool_end_object.
 *
 */
int dm_pool_begin_object(struct dm_pool *p, size_t hint);
int dm_pool_grow_object(struct dm_pool *p, const void *extra, size_t delta);
void *dm_pool_end_object(struct dm_pool *p);
void dm_pool_abandon_object(struct dm_pool *p);

/* utilities */
char *dm_pool_strdup(struct dm_pool *p, const char *str);
char *dm_pool_strndup(struct dm_pool *p, const char *str, size_t n);
void *dm_pool_zalloc(struct dm_pool *p, size_t s);

/******************
 * bitset functions
 ******************/

typedef uint32_t *dm_bitset_t;

dm_bitset_t dm_bitset_create(struct dm_pool *mem, unsigned num_bits);
void dm_bitset_destroy(dm_bitset_t bs);

void dm_bit_union(dm_bitset_t out, dm_bitset_t in1, dm_bitset_t in2);
int dm_bit_get_first(dm_bitset_t bs);
int dm_bit_get_next(dm_bitset_t bs, int last_bit);

#define DM_BITS_PER_INT (sizeof(int) * CHAR_BIT)

#define dm_bit(bs, i) \
   (bs[(i / DM_BITS_PER_INT) + 1] & (0x1 << (i & (DM_BITS_PER_INT - 1))))

#define dm_bit_set(bs, i) \
   (bs[(i / DM_BITS_PER_INT) + 1] |= (0x1 << (i & (DM_BITS_PER_INT - 1))))

#define dm_bit_clear(bs, i) \
   (bs[(i / DM_BITS_PER_INT) + 1] &= ~(0x1 << (i & (DM_BITS_PER_INT - 1))))

#define dm_bit_set_all(bs) \
   memset(bs + 1, -1, ((*bs / DM_BITS_PER_INT) + 1) * sizeof(int))

#define dm_bit_clear_all(bs) \
   memset(bs + 1, 0, ((*bs / DM_BITS_PER_INT) + 1) * sizeof(int))

#define dm_bit_copy(bs1, bs2) \
   memcpy(bs1 + 1, bs2 + 1, ((*bs1 / DM_BITS_PER_INT) + 1) * sizeof(int))

/****************
 * hash functions
 ****************/

struct dm_hash_table;
struct dm_hash_node;

typedef void (*dm_hash_iterate_fn) (void *data);

struct dm_hash_table *dm_hash_create(unsigned size_hint);
void dm_hash_destroy(struct dm_hash_table *t);
void dm_hash_wipe(struct dm_hash_table *t);

void *dm_hash_lookup(struct dm_hash_table *t, const char *key);
int dm_hash_insert(struct dm_hash_table *t, const char *key, void *data);
void dm_hash_remove(struct dm_hash_table *t, const char *key);

void *dm_hash_lookup_binary(struct dm_hash_table *t, const char *key, uint32_t len);
int dm_hash_insert_binary(struct dm_hash_table *t, const char *key, uint32_t len,
		       void *data);
void dm_hash_remove_binary(struct dm_hash_table *t, const char *key, uint32_t len);

unsigned dm_hash_get_num_entries(struct dm_hash_table *t);
void dm_hash_iter(struct dm_hash_table *t, dm_hash_iterate_fn f);

char *dm_hash_get_key(struct dm_hash_table *t, struct dm_hash_node *n);
void *dm_hash_get_data(struct dm_hash_table *t, struct dm_hash_node *n);
struct dm_hash_node *dm_hash_get_first(struct dm_hash_table *t);
struct dm_hash_node *dm_hash_get_next(struct dm_hash_table *t, struct dm_hash_node *n);

#define dm_hash_iterate(v, h) \
	for (v = dm_hash_get_first(h); v; \
	     v = dm_hash_get_next(h, v))

#endif				/* LIB_DEVICE_MAPPER_H */

/*********
 * selinux
 *********/
int dm_set_selinux_context(const char *path, mode_t mode);
