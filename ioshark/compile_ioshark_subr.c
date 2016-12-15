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

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include "ioshark.h"
#include "compile_ioshark.h"

extern char *progname;

static struct files_db_s *files_db_buckets[FILE_DB_HASHSIZE];
static int current_fileno = 1;
static int num_objects = 0;

void
files_db_write_objects(FILE *fp)
{
	int i;
	struct ioshark_file_state st;

	for (i = 0 ; i < FILE_DB_HASHSIZE ; i++) {
		struct files_db_s *db_node, *s;

		db_node = files_db_buckets[i];
		while (db_node != NULL) {
			st.fileno = db_node->fileno;
			st.size = db_node->size;
			if (fwrite(&st, sizeof(st), 1, fp) != 1) {
				fprintf(stderr, "%s Write error trace.outfile\n",
					progname);
				exit(EXIT_FAILURE);
			}
			s = db_node;
			db_node = db_node->next;
			free(s->filename);
			free(s);
		}
	}
}

void *files_db_lookup(char *pathname)
{
	u_int32_t hash;
	struct files_db_s *db_node;

	hash = jenkins_one_at_a_time_hash(pathname, strlen(pathname));
	hash %= FILE_DB_HASHSIZE;
	db_node = files_db_buckets[hash];
	while (db_node != NULL) {
		if (strcmp(db_node->filename, pathname) == 0)
			break;
		db_node = db_node->next;
	}
	return db_node;
}

void *files_db_add(char *filename)
{
	u_int32_t hash;
	struct files_db_s *db_node;

	if ((db_node = files_db_lookup(filename)))
		return db_node;
	hash = jenkins_one_at_a_time_hash(filename, strlen(filename));
	hash %= FILE_DB_HASHSIZE;
	db_node = malloc(sizeof(struct files_db_s));
	db_node->filename = strdup(filename);
	db_node->fileno = current_fileno++;
	db_node->next = files_db_buckets[hash];
	db_node->size = 0;
	files_db_buckets[hash] = db_node;
	num_objects++;
	return db_node;
}

int
files_db_get_total_obj(void)
{
	return num_objects;
}



