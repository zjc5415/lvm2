/*
 * Copyright (C) 2001 Sistina Software (UK) Limited.
 *
 * This file is released under the LGPL.
 */

#ifndef _LVM_TEXT_IMPORT_EXPORT_H
#define _LVM_TEXT_IMPORT_EXPORT_H

#include "config.h"

enum {
	VG_FLAGS,
	PV_FLAGS,
	LV_FLAGS
};

int print_flags(uint32_t status, int type, char *buffer, size_t size);
int read_flags(uint32_t *status, int type, struct config_value *cv);


int text_vg_export(FILE *fp, struct volume_group *vg);
struct volume_group *text_vg_import(struct pool *mem, struct config_file *cf);

#endif
