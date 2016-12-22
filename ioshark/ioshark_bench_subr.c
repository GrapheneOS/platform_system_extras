/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include "ioshark.h"
#include "ioshark_bench.h"

extern char *progname;

void *
files_db_create_handle(void)
{
	struct files_db_handle *h;
	int i;

	h = malloc(sizeof(struct files_db_handle));
	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++)
		h->files_db_buckets[i] = NULL;
	return h;
}

void *files_db_lookup_byfileno(void *handle, int fileno)
{
	u_int32_t	hash;
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node;

	hash = fileno % FILE_DB_HASHSIZE;
	db_node = h->files_db_buckets[hash];
	while (db_node != NULL) {
		if (db_node->fileno == fileno)
			break;
		db_node = db_node->next;
	}
	return db_node;
}

void *files_db_add_byfileno(void *handle, int fileno)
{
	u_int32_t	hash = fileno % FILE_DB_HASHSIZE;
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node;

	db_node = (struct files_db_s *)
		files_db_lookup_byfileno(handle, fileno);
	if (db_node == NULL) {
		db_node = malloc(sizeof(struct files_db_s));
		db_node->fileno = fileno;
		db_node->filename = NULL;
		db_node->size = 0;
		db_node->fd = -1;
		db_node->next = h->files_db_buckets[hash];
		h->files_db_buckets[hash] = db_node;
	} else {
		fprintf(stderr,
			"%s: Node to be added already exists fileno = %d\n\n",
			__func__, fileno);
		exit(EXIT_FAILURE);
	}
	return db_node;
}

void
files_db_fsync_discard_files(void *handle)
{
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node;
	int i;

	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++) {
		db_node = h->files_db_buckets[i];
		while (db_node != NULL) {
			int do_close = 0;

			if (db_node->fd == -1) {
				int fd;

				/*
				 * File was closed, let's open it so we can
				 * fsync and fadvise(DONTNEED) it.
				 */
				do_close = 1;
				fd = open(files_db_get_filename(db_node),
					  O_RDWR);
				if (fd < 0) {
					fprintf(stderr,
						"%s: open(%s O_RDWR) error %d\n",
						progname, db_node->filename,
						errno);
					exit(EXIT_FAILURE);
				}
				db_node->fd = fd;
			}
			if (fsync(db_node->fd) < 0) {
				fprintf(stderr, "%s: Cannot fsync %s\n",
					__func__, db_node->filename);
				exit(1);
			}
			if (posix_fadvise(db_node->fd, 0, 0,
					  POSIX_FADV_DONTNEED) < 0) {
				fprintf(stderr,
					"%s: Cannot fadvise(DONTNEED) %s\n",
					__func__, db_node->filename);
				exit(1);
			}
			if (do_close) {
				close(db_node->fd);
				db_node->fd = -1;
			}
			db_node = db_node->next;
		}
	}
}

void
files_db_update_fd(void *node, int fd)
{
	struct files_db_s *db_node = (struct files_db_s *)node;

	db_node->fd = fd;
}

void
files_db_close_fd(void *node)
{
	struct files_db_s *db_node = (struct files_db_s *)node;

	if (db_node->fd != -1)
		close(db_node->fd);
	db_node->fd = -1;
}

void
files_db_close_files(void *handle)
{
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node;
	int i;

	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++) {
		db_node = h->files_db_buckets[i];
		while (db_node != NULL) {
			if ((db_node->fd != -1) && close(db_node->fd) < 0) {
				fprintf(stderr, "%s: Cannot close %s\n",
					__func__, db_node->filename);
				exit(1);
			}
			db_node->fd = -1;
			db_node = db_node->next;
		}
	}
}

void
files_db_unlink_files(void *handle)
{
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node;
	int i;

	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++) {
		db_node = h->files_db_buckets[i];
		while (db_node != NULL) {
			if ((db_node->fd != -1) && close(db_node->fd) < 0) {
				fprintf(stderr, "%s: Cannot close %s\n",
					__func__, db_node->filename);
				exit(1);
			}
			db_node->fd = -1;
			if (unlink(db_node->filename) < 0) {
				fprintf(stderr, "%s: Cannot unlink %s:%s\n",
					__func__, db_node->filename, strerror(errno));
				exit(1);
			}
			db_node = db_node->next;
		}
	}
}

void
files_db_free_memory(void *handle)
{
	struct files_db_handle *h = (struct files_db_handle *)handle;
	struct files_db_s *db_node, *tmp;
	int i;

	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++) {
		db_node = h->files_db_buckets[i];
		while (db_node != NULL) {
			tmp = db_node;
			db_node = db_node->next;
			free(tmp->filename);
			free(tmp);
		}
	}
	free(h);
}

char *
get_buf(char **buf, int *buflen, int len, int do_fill)
{
	if (len == 0 && *buf == NULL) {
		/*
		 * If we ever get a zero len
		 * request, start with MINBUFLEN
		 */
		if (*buf == NULL)
			len = MINBUFLEN / 2;
	}
	if (*buflen < len) {
		*buflen = MAX(MINBUFLEN, len * 2);
		if (*buf)
			free(*buf);
		*buf = malloc(*buflen);
	}
	if (do_fill) {
		u_int32_t *s;
		int count;

		s = (u_int32_t *)*buf;
		count = *buflen / sizeof(u_int32_t);
		while (count > 0) {
			*s++ = rand();
			count--;
		}
	}
	assert(*buf != NULL);
	return *buf;
}

void
create_file(char *path, size_t size, struct timeval *total_time,
	    struct rw_bytes_s *rw_bytes)
{
	int fd, n;
	char *buf = NULL;
	int buflen = 0;
        struct timeval start;

	(void)gettimeofday(&start, (struct timezone *)NULL);
	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	update_delta_time(&start, total_time);
	if (fd < 0) {
		fprintf(stderr, "%s Cannot create file %s, error = %d\n",
			progname, path, errno);
		exit(EXIT_FAILURE);
	}
	while (size > 0) {
		n = MIN(size, MINBUFLEN);
		buf = get_buf(&buf, &buflen, n, 1);
		(void)gettimeofday(&start, (struct timezone *)NULL);
		if (write(fd, buf, n) < n) {
			fprintf(stderr,
				"%s Cannot write file %s, error = %d\n",
				progname, path, errno);
			exit(EXIT_FAILURE);
		}
		rw_bytes->bytes_written += n;
		update_delta_time(&start, total_time);
		size -= n;
	}
	(void)gettimeofday(&start, (struct timezone *)NULL);
	if (fsync(fd) < 0) {
		fprintf(stderr, "%s Cannot fsync file %s, error = %d\n",
			progname, path, errno);
		exit(EXIT_FAILURE);
	}
	if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) < 0) {
		fprintf(stderr,
			"%s Cannot fadvise(DONTNEED) file %s, error = %d\n",
			progname, path, errno);
		exit(EXIT_FAILURE);
	}
	close(fd);
	update_delta_time(&start, total_time);
}

void
print_op_stats(u_int64_t *op_counts)
{
	int i;
	extern char *IO_op[];

	printf("IO Operation counts :\n");
	for (i = IOSHARK_LSEEK ; i < IOSHARK_MAX_FILE_OP ; i++) {
		printf("%s: %llu\n",
		       IO_op[i], (unsigned long long)op_counts[i]);
	}
}

void
print_bytes(char *desc, struct rw_bytes_s *rw_bytes)
{
	printf("%s: Reads = %dMB, Writes = %dMB\n",
	       desc,
	       (int)(rw_bytes->bytes_read / (1024 * 1024)),
	       (int)(rw_bytes->bytes_written / (1024 * 1024)));
}

