/*
 * util.h
 *
 * Copyright (C) 2014 LSI Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __UTIL__H__
#define __UTIL__H__

#define __user
#include <mtd/mtd-user.h>

uint32_t get_crc32(void *, unsigned long);
int get_mtd_partition_info(const char *, struct mtd_info_user *);
int get_mtd_partition(void *, unsigned long, const char *);

#endif /* __UTIL__H__ */
