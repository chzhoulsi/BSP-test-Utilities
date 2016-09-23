/*
 * util.c
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stropts.h>
#include <errno.h>

#include "util.h"

/*
  ==============================================================================
  Global Variables
  ==============================================================================
*/

static unsigned long crc32_look_up_table_[256] = {
	/*   0 -- */           0u, 1996959894u, 3993919788u, 2567524794u, 
	/*   4 -- */   124634137u, 1886057615u, 3915621685u, 2657392035u, 
	/*   8 -- */   249268274u, 2044508324u, 3772115230u, 2547177864u, 
	/*  12 -- */   162941995u, 2125561021u, 3887607047u, 2428444049u, 
	/*  16 -- */   498536548u, 1789927666u, 4089016648u, 2227061214u, 
	/*  20 -- */   450548861u, 1843258603u, 4107580753u, 2211677639u, 
	/*  24 -- */   325883990u, 1684777152u, 4251122042u, 2321926636u, 
	/*  28 -- */   335633487u, 1661365465u, 4195302755u, 2366115317u, 
	/*  32 -- */   997073096u, 1281953886u, 3579855332u, 2724688242u, 
	/*  36 -- */  1006888145u, 1258607687u, 3524101629u, 2768942443u, 
	/*  40 -- */   901097722u, 1119000684u, 3686517206u, 2898065728u, 
	/*  44 -- */   853044451u, 1172266101u, 3705015759u, 2882616665u, 
	/*  48 -- */   651767980u, 1373503546u, 3369554304u, 3218104598u, 
	/*  52 -- */   565507253u, 1454621731u, 3485111705u, 3099436303u, 
	/*  56 -- */   671266974u, 1594198024u, 3322730930u, 2970347812u, 
	/*  60 -- */   795835527u, 1483230225u, 3244367275u, 3060149565u, 
	/*  64 -- */  1994146192u,   31158534u, 2563907772u, 4023717930u, 
	/*  68 -- */  1907459465u,  112637215u, 2680153253u, 3904427059u, 
	/*  72 -- */  2013776290u,  251722036u, 2517215374u, 3775830040u, 
	/*  76 -- */  2137656763u,  141376813u, 2439277719u, 3865271297u, 
	/*  80 -- */  1802195444u,  476864866u, 2238001368u, 4066508878u, 
	/*  84 -- */  1812370925u,  453092731u, 2181625025u, 4111451223u, 
	/*  88 -- */  1706088902u,  314042704u, 2344532202u, 4240017532u, 
	/*  92 -- */  1658658271u,  366619977u, 2362670323u, 4224994405u, 
	/*  96 -- */  1303535960u,  984961486u, 2747007092u, 3569037538u, 
	/* 100 -- */  1256170817u, 1037604311u, 2765210733u, 3554079995u, 
	/* 104 -- */  1131014506u,  879679996u, 2909243462u, 3663771856u, 
	/* 108 -- */  1141124467u,  855842277u, 2852801631u, 3708648649u, 
	/* 112 -- */  1342533948u,  654459306u, 3188396048u, 3373015174u, 
	/* 116 -- */  1466479909u,  544179635u, 3110523913u, 3462522015u, 
	/* 120 -- */  1591671054u,  702138776u, 2966460450u, 3352799412u, 
	/* 124 -- */  1504918807u,  783551873u, 3082640443u, 3233442989u, 
	/* 128 -- */  3988292384u, 2596254646u,   62317068u, 1957810842u, 
	/* 132 -- */  3939845945u, 2647816111u,   81470997u, 1943803523u, 
	/* 136 -- */  3814918930u, 2489596804u,  225274430u, 2053790376u, 
	/* 140 -- */  3826175755u, 2466906013u,  167816743u, 2097651377u, 
	/* 144 -- */  4027552580u, 2265490386u,  503444072u, 1762050814u, 
	/* 148 -- */  4150417245u, 2154129355u,  426522225u, 1852507879u, 
	/* 152 -- */  4275313526u, 2312317920u,  282753626u, 1742555852u, 
	/* 156 -- */  4189708143u, 2394877945u,  397917763u, 1622183637u, 
	/* 160 -- */  3604390888u, 2714866558u,  953729732u, 1340076626u, 
	/* 164 -- */  3518719985u, 2797360999u, 1068828381u, 1219638859u, 
	/* 168 -- */  3624741850u, 2936675148u,  906185462u, 1090812512u, 
	/* 172 -- */  3747672003u, 2825379669u,  829329135u, 1181335161u, 
	/* 176 -- */  3412177804u, 3160834842u,  628085408u, 1382605366u, 
	/* 180 -- */  3423369109u, 3138078467u,  570562233u, 1426400815u, 
	/* 184 -- */  3317316542u, 2998733608u,  733239954u, 1555261956u, 
	/* 188 -- */  3268935591u, 3050360625u,  752459403u, 1541320221u, 
	/* 192 -- */  2607071920u, 3965973030u, 1969922972u,   40735498u, 
	/* 196 -- */  2617837225u, 3943577151u, 1913087877u,   83908371u, 
	/* 200 -- */  2512341634u, 3803740692u, 2075208622u,  213261112u, 
	/* 204 -- */  2463272603u, 3855990285u, 2094854071u,  198958881u, 
	/* 208 -- */  2262029012u, 4057260610u, 1759359992u,  534414190u, 
	/* 212 -- */  2176718541u, 4139329115u, 1873836001u,  414664567u, 
	/* 216 -- */  2282248934u, 4279200368u, 1711684554u,  285281116u, 
	/* 220 -- */  2405801727u, 4167216745u, 1634467795u,  376229701u, 
	/* 224 -- */  2685067896u, 3608007406u, 1308918612u,  956543938u, 
	/* 228 -- */  2808555105u, 3495958263u, 1231636301u, 1047427035u, 
	/* 232 -- */  2932959818u, 3654703836u, 1088359270u,  936918000u, 
	/* 236 -- */  2847714899u, 3736837829u, 1202900863u,  817233897u, 
	/* 240 -- */  3183342108u, 3401237130u, 1404277552u,  615818150u, 
	/* 244 -- */  3134207493u, 3453421203u, 1423857449u,  601450431u, 
	/* 248 -- */  3009837614u, 3294710456u, 1567103746u,  711928724u, 
	/* 252 -- */  3020668471u, 3272380065u, 1510334235u,  755167117u
};

/*
  ==============================================================================
  Public Implementation
  ==============================================================================
*/

/*
  ------------------------------------------------------------------------------
  get_crc32
*/

uint32_t
get_crc32(void *start, unsigned long size)
{
	unsigned long crc = 0xffffffffUL;
	unsigned long index;
	unsigned char *data = start;

	for (index = 0; index < size; index++) {
		unsigned long temp = (crc ^ *(data++)) & 0x000000ff;
		crc = ((crc >> 8) & 0x00ffffff) ^ crc32_look_up_table_[temp];
	}

	return ~crc;
}

/*
  ------------------------------------------------------------------------------
  get_partition_size_
*/

int
get_mtd_partition_info(const char *partition,
		       struct mtd_info_user *mtd_info)
{
	int fd;

	if (0 > (fd = open(partition, O_RDWR))) {
		fprintf(stderr, "Unable to open %s : %s\n",
			partition, strerror(errno));

		return -1;
	}

	if (0 > ioctl(fd, MEMGETINFO, mtd_info)) {
		fprintf(stderr, "ioctl() failed on %s : %s\n",
			partition, strerror(errno));

		return -1;
	}

	close(fd);

	return 0;
}

/*
  ------------------------------------------------------------------------------
  get_mtd_partition
*/

int
get_mtd_partition(void *output, unsigned long size, const char *partition)
{
	int fd;
    int tmp;

	if (0 > (fd = open(partition, O_RDWR))) {
		fprintf(stderr, "Unable to open %s : %s\n",
			partition, strerror(errno));

		return -1;
	}
	if (0 > (tmp = lseek(fd, 0, SEEK_SET))) {
		fprintf(stderr, "Unable to rewind partition: %s\n",
			strerror(errno));
		close(fd);

		return -1;
	}

	if (size != (tmp=read(fd, output, size))) {
		fprintf(stderr, "Unable to read the partition : %s\n",
			strerror(errno));
		close(fd);

		return -1;
	}

	close(fd);

	return 0;
}
