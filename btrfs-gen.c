#define _XOPEN_SOURCE 9000
#define _GNU_SOURCE
#include <stdint.h>
#include <unistd.h>
//#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
//#include <btrfs/ioctl.h>
//#include <btrfs/rbtree.h>
//#include <btrfs/btrfs-list.h>
#include <btrfs/ctree.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define MEBI (1024*1024)

int64_t btrfs_iterate_tree(int fd, uint64_t tree, void *private, int (*callback)(void*, struct btrfs_ioctl_search_header*, void*)) {
	assert(0<=fd);
	assert(NULL != callback);
	struct btrfs_ioctl_search_args_v2 *args=calloc(MEBI+offsetof(struct btrfs_ioctl_search_args_v2,buf),1);
	if (NULL == args)
		return -ENOMEM;
	args->key.tree_id=tree;
	args->key.max_objectid=-1ULL;
	args->key.max_type=-1U;
	args->key.max_offset=-1ULL;
	args->key.max_transid=-1ULL;
	args->buf_size=MEBI;
	struct btrfs_ioctl_search_header *sh;
	int64_t ret=0;

	do {
		args->key.nr_items=-1U;
		if (ioctl(fd, BTRFS_IOC_TREE_SEARCH_V2, args)) {
			ret=-errno;
			goto out;
		}
		//assume Buffer of MEBI does not fit MEBI items
		assert(MEBI > args->key.nr_items);
		if (0 == args->key.nr_items)
			break;

		sh=(struct btrfs_ioctl_search_header*)args->buf;
		for (uint64_t i=0; i < args->key.nr_items; i++) {
			char *temp=(char*)(sh+1);
			if ((ret=callback(temp, sh, private)))
				goto out;

			args->key.min_offset=sh->offset+1;
			args->key.min_type=sh->type;
			args->key.min_objectid=sh->objectid;
			sh=(struct btrfs_ioctl_search_header*)(sh->len+temp);
		}
		ret+=args->key.nr_items;
	} while (1);

out:
	free(args);
	return ret;
}

static int get_generation_cb(void *data, struct btrfs_ioctl_search_header *sh, void *private) {
	(void)private;
	if (BTRFS_ROOT_ITEM_KEY != sh->type )
		return 0;
	assert(sizeof(struct btrfs_root_item) <=sh->len);
	struct btrfs_root_item *ritem=(struct btrfs_root_item*)data;
	printf("%lld\t%lld\n", sh->objectid, ritem->generation);

	return 0;
}

int64_t btrfs_get_generations(int fd) {
	int64_t ret=btrfs_iterate_tree(fd, BTRFS_ROOT_TREE_OBJECTID, NULL, get_generation_cb);
	if (0>ret)
		return ret;
	return 0;
}

int main(int argc, char **argv) {
	if (2 != argc) {
		fprintf(stderr, "Usage %s <path>\n", argv[0]);
		exit(1);
	}

	int fd = open(argv[1], O_RDONLY);
	if (0 > fd) {
		fprintf(stderr, "Path %s not found.\n", argv[1]);
		exit(1);
	}
	btrfs_get_generations(fd);
	close(fd);

	return EXIT_SUCCESS;
}
