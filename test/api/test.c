/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2009 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "lvm.h"

#define MAX_ARGS 64

static int lvm_split(char *str, int *argc, char **argv, int max)
{
	char *b = str, *e;
	*argc = 0;

	while (*b) {
		while (*b && isspace(*b))
			b++;

		if ((!*b) || ((*argc == 0)&&(*b == '#')))
			break;

		e = b;
		while (*e && !isspace(*e))
			e++;

		argv[(*argc)++] = b;
		if (!*e)
			break;
		*e++ = '\0';
		b = e;
		if (*argc == max)
			break;
	}

	return *argc;
}

static void _show_help(void)
{
	printf("'vg_create_lv_linear vgname lvname size_in_bytes': "
	       "Create a linear LV\n");
	printf("'scan_vgs': "
	       "Scan the system for LVM metadata\n");
	printf("'vg_list_names': "
	       "List the names of the VGs that exist in the system\n");
	printf("'vg_list_ids': "
	       "List the uuids of the VGs that exist in the system\n");
	printf("'vg_list_pvs vgname': "
	       "List the PVs that exist in VG vgname\n");
	printf("'vg_list_lvs vgname': "
	       "List the LVs that exist in VG vgname\n");
	printf("'vgs_open': "
	       "List the VGs that are currently open\n");
	printf("'vgs': "
	       "List all VGs known to the system\n");
	printf("'vg_open vgname ['r' | 'w']': "
	       "Issue a lvm_vg_open() API call on VG 'vgname'\n");
	printf("'vg_close vgname': "
	       "Issue a lvm_vg_close() API call on VG 'vgname'\n");
	printf("'quit': exit the program\n");
}

static struct dm_hash_table *_vgid_hash = NULL;
static struct dm_hash_table *_vgname_hash = NULL;
static struct dm_hash_table *_pvname_hash = NULL;
static struct dm_hash_table *_lvname_hash = NULL;

static void _hash_destroy_single(struct dm_hash_table **htable)
{
	if (htable && *htable) {
		dm_hash_destroy(*htable);
		*htable = NULL;
	}
}

static void _hash_destroy(void)
{
	_hash_destroy_single(&_vgname_hash);
	_hash_destroy_single(&_vgid_hash);
	_hash_destroy_single(&_pvname_hash);
	_hash_destroy_single(&_lvname_hash);
}

static int _hash_create(void)
{
	if (!(_vgname_hash = dm_hash_create(128)))
		return 0;
	if (!(_pvname_hash = dm_hash_create(128))) {
		_hash_destroy_single(&_vgname_hash);
		return 0;
	}
	if (!(_lvname_hash = dm_hash_create(128))) {
		_hash_destroy_single(&_vgname_hash);
		_hash_destroy_single(&_pvname_hash);
		return 0;
	}
	if (!(_vgid_hash = dm_hash_create(128))) {
		_hash_destroy_single(&_vgname_hash);
		_hash_destroy_single(&_pvname_hash);
		_hash_destroy_single(&_lvname_hash);
		return 0;
	}
	return 1;
}

static vg_t *_lookup_vg_by_name(char **argv, int argc)
{
	vg_t *vg;

	if (argc < 2) {
		printf ("Please enter vg_name\n");
		return NULL;
	}
	if (!(vg = dm_hash_lookup(_vgid_hash, argv[1])) &&
	    !(vg = dm_hash_lookup(_vgname_hash, argv[1]))) {
		printf ("Can't find %s in open VGs - run vg_open first\n",
			argv[1]);
		return NULL;
	}
	return vg;
}
static void _add_lvs_to_lvname_hash(struct dm_list *lvs)
{
	struct lvm_lv_list *lvl;
	dm_list_iterate_items(lvl, lvs) {
		/* Concatenate VG name with LV name */
		dm_hash_insert(_lvname_hash, lvm_lv_get_name(lvl->lv), lvl->lv);
	}
}

static void _add_pvs_to_pvname_hash(struct dm_list *pvs)
{
	struct lvm_pv_list *pvl;
	dm_list_iterate_items(pvl, pvs) {
		dm_hash_insert(_pvname_hash, lvm_pv_get_name(pvl->pv), pvl->pv);
	}
}

static void _vg_open(char **argv, int argc, lvm_t libh)
{
	vg_t *vg;
	struct dm_list *lvs;
	struct dm_list *pvs;

	if (argc < 2) {
		printf ("Please enter vg_name\n");
		return;
	}
	if ((vg = dm_hash_lookup(_vgid_hash, argv[1])) ||
	    (vg = dm_hash_lookup(_vgname_hash, argv[1]))) {
		printf ("VG already open\n");
		return;
	}
	if (argc < 3)
		vg = lvm_vg_open(libh, argv[1], "r", 0);
	else
		vg = lvm_vg_open(libh, argv[1], argv[2], 0);
	if (!vg || !lvm_vg_get_name(vg)) {
		printf("Error opening %s\n", argv[1]);
		return;
	}

	printf("Success opening vg %s\n", argv[1]);
	dm_hash_insert(_vgname_hash, lvm_vg_get_name(vg), vg);
	dm_hash_insert(_vgid_hash, lvm_vg_get_uuid(vg), vg);

	/*
	 * Add the LVs and PVs into the hashes for lookups
	 */
	lvs = lvm_vg_list_lvs(vg);
	if (lvs && !dm_list_empty(lvs))
		_add_lvs_to_lvname_hash(lvs);
	pvs = lvm_vg_list_pvs(vg);
	if (pvs && !dm_list_empty(pvs))
		_add_pvs_to_pvname_hash(pvs);
}

static void _vg_close(char **argv, int argc)
{
	vg_t *vg;

	if (argc < 2) {
		printf ("Please enter vg_name\n");
		return;
	}
	while((vg = dm_hash_lookup(_vgname_hash, argv[1]))) {
		dm_hash_remove(_vgid_hash, lvm_vg_get_uuid(vg));
		dm_hash_remove(_vgname_hash, lvm_vg_get_name(vg));
		lvm_vg_close(vg);
	}
	while((vg = dm_hash_lookup(_vgid_hash, argv[1]))) {
		dm_hash_remove(_vgid_hash, lvm_vg_get_uuid(vg));
		dm_hash_remove(_vgname_hash, lvm_vg_get_name(vg));
		lvm_vg_close(vg);
	}
}

static void _show_one_vg(vg_t *vg)
{
	printf("%s (%s): size=%"PRIu64", free=%"PRIu64", #pv=%"PRIu64"\n",
		lvm_vg_get_name(vg), lvm_vg_get_uuid(vg),
		lvm_vg_get_size(vg), lvm_vg_get_free(vg),
		lvm_vg_get_pv_count(vg));
}

static void _list_open_vgs(void)
{
	dm_hash_iter(_vgid_hash, (dm_hash_iterate_fn) _show_one_vg);
}

static void _pvs_in_vg(char **argv, int argc)
{
	struct dm_list *pvs;
	struct lvm_pv_list *pvl;
	vg_t *vg;

	if (!(vg = _lookup_vg_by_name(argv, argc)))
		return;
	pvs = lvm_vg_list_pvs(vg);
	if (!pvs || dm_list_empty(pvs)) {
		printf("No PVs in VG %s\n", lvm_vg_get_name(vg));
		return;
	}
	printf("PVs in VG %s:\n", lvm_vg_get_name(vg));
	dm_list_iterate_items(pvl, pvs) {
		printf("%s (%s): mda_count=%"PRIu64"\n",
		       lvm_pv_get_name(pvl->pv), lvm_pv_get_uuid(pvl->pv),
			lvm_pv_get_mda_count(pvl->pv));
	}
}

static void _scan_vgs(lvm_t libh)
{
	lvm_scan_vgs(libh);
}

static void _vg_list_names(lvm_t libh)
{
	struct dm_list *list;
	struct lvm_str_list *strl;
	const char *tmp;

	list = lvm_list_vg_names(libh);
	printf("VG names:\n");
	dm_list_iterate_items(strl, list) {
		tmp = strl->str;
		printf("%s\n", tmp);
	}
}

static void _vg_list_ids(lvm_t libh)
{
	struct dm_list *list;
	struct lvm_str_list *strl;
	const char *tmp;

	list = lvm_list_vg_ids(libh);
	printf("VG uuids:\n");
	dm_list_iterate_items(strl, list) {
		tmp = strl->str;
		printf("%s\n", tmp);
	}
}


static void _lvs_in_vg(char **argv, int argc)
{
	struct dm_list *lvs;
	struct lvm_lv_list *lvl;
	vg_t *vg;

	if (!(vg = _lookup_vg_by_name(argv, argc)))
		return;
	lvs = lvm_vg_list_lvs(vg);
	if (!lvs || dm_list_empty(lvs)) {
		printf("No LVs in VG %s\n", lvm_vg_get_name(vg));
		return;
	}
	printf("LVs in VG %s:\n", lvm_vg_get_name(vg));
	dm_list_iterate_items(lvl, lvs) {
		printf("%s/%s (%s): size=%"PRIu64"\n", lvm_vg_get_name(vg),
			lvm_lv_get_name(lvl->lv), lvm_lv_get_uuid(lvl->lv),
			lvm_lv_get_size(lvl->lv));
	}
}

static void _vg_create_lv_linear(char **argv, int argc)
{
	vg_t *vg;
	lv_t *lv;

	if (argc < 4) {
		printf("Please enter vgname, lvname, and size\n");
		return;
	}
	if (!(vg = _lookup_vg_by_name(argv, argc)))
		return;
	lv = lvm_vg_create_lv_linear(vg, argv[2], atol(argv[3]));
	if (!lv)
		printf("Error ");
	else
		printf("Success ");
	printf("creating LV %s in VG %s\n",
		argv[2], lvm_vg_get_name(vg));
}

static int lvmapi_test_shell(lvm_t libh)
{
	int argc;
	char *input = NULL, *args[MAX_ARGS], **argv;

	_hash_create();
	argc=0;
	while (1) {
		free(input);
		input = readline("lvm> ");

		/* EOF */
		if (!input) {
			printf("\n");
			break;
		}

		/* empty line */
		if (!*input)
			continue;

		argv = args;

		if (lvm_split(input, &argc, argv, MAX_ARGS) == MAX_ARGS) {
			printf("Too many arguments, sorry.");
			continue;
		}

		if (!strcmp(argv[0], "lvm")) {
			argv++;
			argc--;
		}

		if (!argc)
			continue;

		if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) {
			printf("Exiting.\n");
			break;
		} else if (!strcmp(argv[0], "?") || !strcmp(argv[0], "help")) {
			_show_help();
		} else if (!strcmp(argv[0], "vg_open")) {
			_vg_open(argv, argc, libh);
		} else if (!strcmp(argv[0], "vg_close")) {
			_vg_close(argv, argc);
		} else if (!strcmp(argv[0], "vgs_open")) {
			_list_open_vgs();
		} else if (!strcmp(argv[0], "vg_list_pvs")) {
			_pvs_in_vg(argv, argc);
		} else if (!strcmp(argv[0], "vg_list_lvs")) {
			_lvs_in_vg(argv, argc);
		} else if (!strcmp(argv[0], "vg_list_names")) {
			_vg_list_names(libh);
		} else if (!strcmp(argv[0], "vg_list_ids")) {
			_vg_list_ids(libh);
		} else if (!strcmp(argv[0], "scan_vgs")) {
			_scan_vgs(libh);
		} else if (!strcmp(argv[0], "vg_create_lv_linear")) {
			_vg_create_lv_linear(argv, argc);
		} else {
			printf ("Unrecognized command %s\n", argv[0]);
		}
	}

	dm_hash_iter(_vgname_hash, (dm_hash_iterate_fn) lvm_vg_close);
	_hash_destroy();
	free(input);
	return 0;
}

int main (int argc, char *argv[])
{
	lvm_t libh;

	libh = lvm_create(NULL);
	if (!libh) {
		printf("Unable to open lvm library instance\n");
		return 1;
	}

	lvmapi_test_shell(libh);

	lvm_destroy(libh);
	return 0;
}

