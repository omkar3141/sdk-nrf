/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "zms_footprint_dbg.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log_ctrl.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/kvss/zms.h>
#include <zephyr/storage/flash_map.h>
#include <settings/settings_zms.h>

#define ZMS_DATA_IN_ATE_SIZE	8
#define RPL_PREFIX		"bt/mesh/RPL/"
#define RPL_PREFIX_LEN		(sizeof(RPL_PREFIX) - 1)

/* Column widths for aligned printk tables (longest mesh path is 20 chars). */
#define ZMS_FP_PATH_W		21
#define ZMS_FP_LEN_W		9
#define ZMS_FP_VALUE_PREVIEW_B	4
#define ZMS_FP_VALUE_W		8	/* up to 4 bytes -> 8 hex digits */
#define ZMS_FP_ID_W		8
#define ZMS_FP_STORE_W		10
/* "path_flash" is 10 chars; %*s still prints longer strings if width is too small. */
#define ZMS_FP_FLASH_W		10

/* Must match settings_zms.c read_fn_arg passed to load callbacks. */
struct settings_zms_read_fn_arg {
	struct zms_fs *fs;
	uint32_t id;
};

enum zms_fp_table {
	ZMS_FP_TABLE_MESH,
	ZMS_FP_TABLE_RPL,
};

struct zms_fp_ctx {
	struct zms_fs *fs;
	size_t zms_ate_len;
	size_t write_block_size;
	enum zms_fp_table table;
	uint32_t mesh_key_count;
	size_t mesh_sum_row_total;
	uint32_t rpl_key_count;
	size_t rpl_sum_row_total;
};

static size_t zms_obj_flash(struct zms_fs *fs, size_t data_len)
{
	size_t zms_ate_len = fs->ate_size;
	size_t wb = fs->flash_parameters->write_block_size;

	if (data_len > ZMS_DATA_IN_ATE_SIZE) {
		return zms_ate_len + (size_t)ROUND_UP(data_len, wb);
	}

	return zms_ate_len;
}

static size_t zms_mounted_size(struct zms_fs *fs)
{
	return (size_t)fs->sector_size * fs->sector_count;
}

static void zms_store_desc(char *buf, size_t buf_len, size_t data_len, size_t wb)
{
	if (data_len <= ZMS_DATA_IN_ATE_SIZE) {
		snprintf(buf, buf_len, "ATE");
	} else {
		size_t ext = (size_t)ROUND_UP(data_len, wb);

		snprintf(buf, buf_len, "ext %zu", ext);
	}
}

/* First bytes of settings value as hex, printed byte3..byte0 (MSB left). */
static void zms_fp_format_value_preview(struct zms_fs *fs, uint32_t val_id, size_t value_len,
					char *buf, size_t buf_len)
{
	uint8_t bytes[ZMS_FP_VALUE_PREVIEW_B];
	size_t n;
	ssize_t rc;
	char *p = buf;
	size_t rem = buf_len;

	if (buf_len == 0) {
		return;
	}

	buf[0] = '\0';

	if (value_len == 0) {
		snprintf(buf, buf_len, "-");
		return;
	}

	n = MIN(value_len, ZMS_FP_VALUE_PREVIEW_B);
	rc = zms_read(fs, val_id, bytes, n);
	if (rc < 0 || (size_t)rc < n) {
		snprintf(buf, buf_len, "?");
		return;
	}

	for (size_t i = n; i > 0; i--) {
		int w = snprintf(p, rem, "%02x", bytes[i - 1]);

		if (w < 0 || (size_t)w >= rem) {
			break;
		}

		p += w;
		rem -= (size_t)w;
	}
}

static void zms_fp_print_columns(void)
{
	printk("%-*s | %*s | %*s | %*s | %*s | %*s | %*s | %-*s | %-*s | %-*s | "
	       "%*s | %*s | %*s | %*s\n",
	       ZMS_FP_PATH_W, "path",
	       ZMS_FP_LEN_W, "path_len",
	       ZMS_FP_VALUE_W, "value",
	       ZMS_FP_LEN_W, "value_len",
	       ZMS_FP_ID_W, "path_id",
	       ZMS_FP_ID_W, "val_id",
	       ZMS_FP_ID_W, "ll_id",
	       ZMS_FP_STORE_W, "path_store",
	       ZMS_FP_STORE_W, "val_store",
	       ZMS_FP_STORE_W, "ll_store",
	       ZMS_FP_FLASH_W, "path_flash",
	       ZMS_FP_FLASH_W, "val_flash",
	       ZMS_FP_FLASH_W, "ll_flash",
	       ZMS_FP_FLASH_W, "row_total");
}

static void zms_fp_print_table_intro(const char *title)
{
	printk("\n--- %s ---\n", title);
	zms_fp_print_columns();
}

static int zms_fp_emit_row(struct zms_fp_ctx *ctx, const char *name, size_t value_len,
			   const struct settings_zms_read_fn_arg *rd)
{
	uint32_t path_id;
	uint32_t val_id;
	uint32_t ll_id;
	ssize_t path_len;
	ssize_t ll_len;
	size_t path_flash;
	size_t value_flash;
	size_t ll_flash;
	size_t row_total;
	char path_store[16];
	char val_store[16];
	char ll_store[16];
	char val_preview[(ZMS_FP_VALUE_PREVIEW_B * 2) + 1];

	val_id = rd->id;
	path_id = val_id - ZMS_DATA_ID_OFFSET;
	ll_id = ZMS_LL_NODE_FROM_NAME_ID(path_id);

	path_len = zms_get_data_length(ctx->fs, path_id);
	ll_len = zms_get_data_length(ctx->fs, ll_id);

	if (path_len < 0 || (ssize_t)value_len < 0 || ll_len < 0) {
		printk("skip (read err): %s\n", name);
		return 0;
	}

	path_flash = zms_obj_flash(ctx->fs, (size_t)path_len);
	value_flash = zms_obj_flash(ctx->fs, value_len);
	ll_flash = zms_obj_flash(ctx->fs, (size_t)ll_len);
	row_total = path_flash + value_flash + ll_flash;

	zms_store_desc(path_store, sizeof(path_store), (size_t)path_len,
		       ctx->write_block_size);
	zms_store_desc(val_store, sizeof(val_store), value_len, ctx->write_block_size);
	zms_store_desc(ll_store, sizeof(ll_store), (size_t)ll_len, ctx->write_block_size);
	zms_fp_format_value_preview(ctx->fs, val_id, value_len, val_preview,
				    sizeof(val_preview));

	printk("%-*s | %*zd | %*s | %*zu | %08x | %08x | %08x | %-*s | %-*s | %-*s | "
	       "%*zu | %*zu | %*zu | %*zu\n",
	       ZMS_FP_PATH_W, name,
	       ZMS_FP_LEN_W, path_len,
	       ZMS_FP_VALUE_W, val_preview,
	       ZMS_FP_LEN_W, value_len,
	       path_id, val_id, ll_id,
	       ZMS_FP_STORE_W, path_store,
	       ZMS_FP_STORE_W, val_store,
	       ZMS_FP_STORE_W, ll_store,
	       ZMS_FP_FLASH_W, path_flash,
	       ZMS_FP_FLASH_W, value_flash,
	       ZMS_FP_FLASH_W, ll_flash,
	       ZMS_FP_FLASH_W, row_total);

	if (ctx->table == ZMS_FP_TABLE_RPL) {
		ctx->rpl_key_count++;
		ctx->rpl_sum_row_total += row_total;
	} else {
		ctx->mesh_key_count++;
		ctx->mesh_sum_row_total += row_total;
	}

	return 0;
}

static int zms_fp_row_cb(const char *name, size_t len, settings_read_cb read_cb,
			 void *cb_arg, void *param)
{
	struct zms_fp_ctx *ctx = param;
	const struct settings_zms_read_fn_arg *rd;
	bool is_rpl;

	ARG_UNUSED(read_cb);

	is_rpl = !strncmp(name, RPL_PREFIX, RPL_PREFIX_LEN);

	if (ctx->table == ZMS_FP_TABLE_MESH && is_rpl) {
		return 0;
	}

	if (ctx->table == ZMS_FP_TABLE_RPL && !is_rpl) {
		return 0;
	}

	if (!cb_arg) {
		printk("skip (no ZMS cb_arg): %s\n", name);
		return 0;
	}

	rd = cb_arg;

	return zms_fp_emit_row(ctx, name, len, rd);
}

static void zms_fp_scan_table(struct zms_fp_ctx *ctx, enum zms_fp_table table,
			      const char *title)
{
	ctx->table = table;
	zms_fp_print_table_intro(title);
	settings_load_subtree_direct(NULL, zms_fp_row_cb, ctx);
}

void zms_footprint_dbg_dump(void)
{
	void *storage;
	struct zms_fs *fs;
	struct zms_fp_ctx ctx = { 0 };
	const struct flash_area *fa;
	ssize_t free_space;
	size_t partition_size;
	size_t mounted_size;
	size_t partition_tail;
	size_t mounted_used;
	size_t gc_reserve_size;
	size_t overhead_excl_gc;
	size_t free_space_u;
	size_t logical_sum;
	int err;

#if IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)
	log_panic();
#endif

	err = settings_storage_get(&storage);
	if (err || !storage) {
		printk("ZMS footprint: settings_storage_get failed (%d)\n", err);
		return;
	}

	fs = storage;
	if (!fs->ready) {
		printk("ZMS footprint: ZMS not mounted\n");
		return;
	}

	ctx.fs = fs;
	ctx.zms_ate_len = fs->ate_size;
	ctx.write_block_size = fs->flash_parameters->write_block_size;

	printk("\n=== ZMS settings footprint ===\n");
	printk("zms_ate_len=%zu  write_block_size=%zu\n",
	       ctx.zms_ate_len, ctx.write_block_size);
	printk("Per row: path + value + LL ZMS objects (see row_total formula below).\n");
	printk("  path_flash  = zms_ate_len + ROUND_UP(path_len,wb) if path_len>8\n");
	printk("  value_flash = zms_ate_len + ROUND_UP(value_len,wb) if value_len>8\n");
	printk("  ll_flash    = zms_ate_len (LL is 8 B, in ATE)\n");
	printk("  row_total   = path_flash + value_flash + ll_flash\n");
	printk("ZMS IDs: path MSB=10; val_id=path_id+0x40000000; ll_id=path_id|1\n");
	printk("value column: first %u byte(s) as hex, printed byte3..byte0 order.\n",
	       ZMS_FP_VALUE_PREVIEW_B);
	printk("Logical footprint per table; raw partition size, mounted size, and GC reserve\n");
	printk("are shown separately.\n");

	zms_fp_scan_table(&ctx, ZMS_FP_TABLE_MESH,
			  "Mesh settings (non-RPL, bt/mesh except RPL/)");
	zms_fp_scan_table(&ctx, ZMS_FP_TABLE_RPL, "RPL (bt/mesh/RPL/)");

	err = flash_area_open(PARTITION_ID(storage_partition), &fa);
	if (err) {
		printk("Summary: mesh=%u rpl=%u (partition open err %d)\n",
		       ctx.mesh_key_count, ctx.rpl_key_count, err);
		return;
	}

	partition_size = fa->fa_size;
	flash_area_close(fa);

	free_space = zms_calc_free_space(fs);
	if (free_space < 0) {
		printk("Summary: mesh=%u rpl=%u (free_space err %zd)\n",
		       ctx.mesh_key_count, ctx.rpl_key_count, free_space);
		return;
	}

	logical_sum = ctx.mesh_sum_row_total + ctx.rpl_sum_row_total;
	mounted_size = zms_mounted_size(fs);

	if (partition_size < mounted_size) {
		printk("Summary: partition smaller than mounted ZMS (%zu < %zu)\n",
		       partition_size, mounted_size);
		return;
	}

	free_space_u = (size_t)free_space;
	if (free_space_u > mounted_size) {
		printk("Summary: free space larger than mounted ZMS (%zu > %zu)\n",
		       free_space_u, mounted_size);
		return;
	}

	partition_tail = partition_size - mounted_size;
	mounted_used = mounted_size - free_space_u;
	gc_reserve_size = fs->sector_size;
	overhead_excl_gc = 0U;
	if (mounted_used > (logical_sum + gc_reserve_size)) {
		overhead_excl_gc = mounted_used - logical_sum - gc_reserve_size;
	}

	printk("\n--- Summary ---\n");
	printk("mesh keys:           %u\n", ctx.mesh_key_count);
	printk("mesh sum row_total:  %zu B\n", ctx.mesh_sum_row_total);
	printk("RPL keys:            %u\n", ctx.rpl_key_count);
	printk("RPL sum row_total:   %zu B\n", ctx.rpl_sum_row_total);
	printk("logical sum:         %zu B (live rows only)\n", logical_sum);
	printk("\n");
	printk("raw partition size:  %zu B\n", partition_size);
	printk("unmounted tail:      %zu B\n", partition_tail);
	printk("\n");
	printk("mounted ZMS size:    %zu B (%u sectors x %u B)\n", mounted_size,
		fs->sector_count, fs->sector_size);
	printk("gc reserve (1 sect): %zu B\n", gc_reserve_size);
	printk("mounted free:        %zd B\n", free_space);
	printk("mounted used:        %zu B (incl. GC reserve)\n", mounted_used);
	printk("ZMS overhead ex. GC: %zu B\n", overhead_excl_gc);
	printk("=== end ===\n\n");
}
