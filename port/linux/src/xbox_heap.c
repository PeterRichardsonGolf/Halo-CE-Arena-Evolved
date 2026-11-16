/*
XBOX_HEAP.C

The game's heap, inside the Xbox address space.

Everything the game allocates (system_malloc is GlobalAlloc) can end up in
a structure that holds 32-bit pointers: tag data built at run time, script
values, Direct3D resource headers. So the heap lives in the Xbox address
space too (cseries/xbox_address.h), below the contiguous window: Xbox addresses
XBOX_HEAP_BASE to XBOX_HEAP_END, committed read-write at the first allocation
and backed by the host on first touch.

Blocks carry a 16-byte header. Requests up to 64 KB come from power-of-two
size classes with free lists; larger ones are rounded to 64 KB and reuse
freed large blocks first fit. The game allocates mostly at start-up and map
load, so this stays simple rather than clever.
*/

#include "platform.h"
#include "ae_mcc_memory.h" /* AE hook */

#ifdef HALO_64BIT

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef HALO_WINDOW_TRANSLATED
/* the window's part below the contiguous memory (xbox_address.h), committed
as the heap grows: the console has no pages backed on first touch */
#define XBOX_HEAP_BASE XBOX_WINDOW_LOW
#define XBOX_HEAP_END PLATFORM_CONTIGUOUS_BASE
#define HEAP_COMMIT_GRANULARITY 0x100000U
#else
/* (above Custom Edition maps' tag cache and the room above it, platform.h) */
#define XBOX_HEAP_BASE 0x42000000U
#define XBOX_HEAP_END 0x7f000000U
#endif
#define HEAP_MAGIC 0x68656170U /* 'heap' */
#define SMALL_CLASS_COUNT 13 /* 16 bytes .. 64 KB of payload */
#define LARGE_GRANULARITY 0x10000U

typedef char verify_heap_above_ce_tag_cache[PLATFORM_CE_TAG_CACHE_BASE + PLATFORM_CE_TAG_CACHE_SIZE +
	PLATFORM_CE_CONVERSION_ROOM_SIZE <= XBOX_HEAP_BASE ? 1 : -1];

struct heap_block
{
	unsigned long long capacity; /* payload bytes */
	unsigned int magic;
	unsigned int free;
};

struct free_node
{
	struct free_node *next;
};

static pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *heap_next;
static unsigned char *heap_end;
static struct free_node *small_free[SMALL_CLASS_COUNT];
static struct free_node *large_free;
#ifdef HALO_WINDOW_TRANSLATED
/* the end of the committed part */
static unsigned char *heap_committed;
#endif

static BOOL heap_initialize(void)
{
	void *base = xbox_pointer(XBOX_HEAP_BASE);

	if (heap_next)
		return TRUE;
#ifdef HALO_WINDOW_TRANSLATED
	heap_committed = base;
#else
	/* (no swap reserved: nearly 1 GB of address space, of which the game
	touches a small part; a machine with 1 GB of memory refuses it otherwise) */
	if (mmap(base, XBOX_HEAP_END - XBOX_HEAP_BASE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != base)
	{
		platform_log("cannot commit the Xbox heap (%s): the system refused %u MB of address space (a memory "
			"limit such as ulimit -v, or vm.overcommit_memory = 2)", strerror(errno),
			(XBOX_HEAP_END - XBOX_HEAP_BASE) >> 20);
		abort();
	}
#endif
	heap_next = base;
	heap_end = xbox_pointer(XBOX_HEAP_END);
	return TRUE;
}

static int small_class(size_t size)
{
	int index = 0;

	while (index < SMALL_CLASS_COUNT && ((size_t)16 << index) < size)
		index++;
	return index; /* SMALL_CLASS_COUNT: a large block */
}

static struct heap_block *block_of(void *payload)
{
	return (struct heap_block *)payload - 1;
}

static struct heap_block *carve(size_t capacity)
{
	struct heap_block *block;

	heap_next = ae_mcc_heap_skip(heap_next, sizeof(*block) + capacity); /* AE hook */
	if ((size_t)(heap_end - heap_next) < sizeof(*block) + capacity)
		return NULL;
#ifdef HALO_WINDOW_TRANSLATED
	if (heap_next + sizeof(*block) + capacity > heap_committed)
	{
		size_t wanted = (size_t)(heap_next + sizeof(*block) + capacity - heap_committed);
		size_t size = (wanted + HEAP_COMMIT_GRANULARITY - 1) & ~(size_t)(HEAP_COMMIT_GRANULARITY - 1);

		if (size > (size_t)(heap_end - heap_committed))
			size = (size_t)(heap_end - heap_committed);
		if (mmap(heap_committed, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) !=
			heap_committed)
		{
			platform_log("cannot commit %u more bytes of the Xbox heap (%s)", (unsigned int)size, strerror(errno));
			return NULL;
		}
		heap_committed += size;
	}
#endif
	block = (struct heap_block *)heap_next;
	heap_next += sizeof(*block) + capacity;
	block->capacity = capacity;
	block->magic = HEAP_MAGIC;
	return block;
}

/* ---------- (AE 0.7.9) merging: maps of many big textures (a Workshop map pack's) left the heap in pieces. Each map's
textures are freed when it is left, but a freed large block was only reused whole, first fit, for a block no bigger: a
run of maps with other texture sizes carved new space each time until the window was gone ("Xbox heap exhausted"
with most of it free: Digsite's Weld, then every map, on a dedicated server). Large blocks are now taken best fit and
split, and when the heap is out every free block is joined with its free neighbours and a free run at its top is given
back to it, before the allocation is refused. */

/* a free block listed where its capacity belongs (under the lock) */
static void list_free(struct heap_block *block)
{
	struct free_node *node = (struct free_node *)(block + 1);
	int index = small_class((size_t)block->capacity);

	block->free = TRUE;
	if (index < SMALL_CLASS_COUNT && block->capacity == ((size_t)16 << index))
	{
		node->next = small_free[index];
		small_free[index] = node;
	}
	else
	{
		node->next = large_free;
		large_free = node;
	}
}

/* the smallest free large block of the capacity or more, its rest a free block of its own when it holds a large
block's worth (under the lock); NULL when none is big enough */
static struct heap_block *take_large(size_t capacity)
{
	struct free_node **link, **best = NULL;
	struct heap_block *block;

	for (link = &large_free; *link; link = &(*link)->next)
	{
		if (block_of(*link)->capacity >= capacity &&
			(!best || block_of(*link)->capacity < block_of(*best)->capacity))
		{
			best = link;
			if (block_of(*link)->capacity == capacity)
				break;
		}
	}
	if (!best)
		return NULL;
	block = block_of(*best);
	*best = (*best)->next;
	if (block->capacity - capacity >= sizeof(struct heap_block) + LARGE_GRANULARITY)
	{
		struct heap_block *rest = (struct heap_block *)((unsigned char *)(block + 1) + capacity);

		rest->capacity = block->capacity - capacity - sizeof(struct heap_block);
		rest->magic = HEAP_MAGIC;
		block->capacity = capacity;
		list_free(rest);
	}
	return block;
}

static int block_address_order(const void *a, const void *b)
{
	uintptr_t x = (uintptr_t)*(struct heap_block *const *)a;
	uintptr_t y = (uintptr_t)*(struct heap_block *const *)b;

	return x < y ? -1 : x > y;
}

/* every free block joined with the free blocks right after it, a free run that ends where carving goes on given back
to it (under the lock); 1 when anything changed */
static int heap_merge(void)
{
	struct heap_block **blocks;
	struct free_node *node;
	size_t count = 0, index, kept = 0, joined = 0, top = 0;
	int class_index;

	for (class_index = 0; class_index < SMALL_CLASS_COUNT; class_index++)
		for (node = small_free[class_index]; node; node = node->next)
			count++;
	for (node = large_free; node; node = node->next)
		count++;
	if (!count)
		return 0;
	blocks = malloc(count * sizeof(*blocks));
	if (!blocks)
		return 0;
	count = 0;
	for (class_index = 0; class_index < SMALL_CLASS_COUNT; class_index++)
	{
		for (node = small_free[class_index]; node; node = node->next)
			blocks[count++] = block_of(node);
		small_free[class_index] = NULL;
	}
	for (node = large_free; node; node = node->next)
		blocks[count++] = block_of(node);
	large_free = NULL;
	qsort(blocks, count, sizeof(*blocks), block_address_order);
	for (index = 0; index < count; index++)
	{
		struct heap_block *block = blocks[index];

		/* (the window ae_mcc_heap_skip leaves out lies between two blocks: never joined across it) */
		while (index + 1 < count && (unsigned char *)(block + 1) + block->capacity == (unsigned char *)blocks[index + 1])
		{
			block->capacity += sizeof(struct heap_block) + blocks[index + 1]->capacity;
			blocks[index + 1]->magic = 0;
			index++;
			joined++;
		}
		blocks[kept++] = block;
	}
	while (kept && (unsigned char *)(blocks[kept - 1] + 1) + blocks[kept - 1]->capacity == heap_next)
	{
		top += sizeof(struct heap_block) + blocks[kept - 1]->capacity;
		heap_next = (unsigned char *)blocks[kept - 1];
		blocks[kept - 1]->magic = 0;
		kept--;
	}
	for (index = 0; index < kept; index++)
		list_free(blocks[index]);
	free(blocks);
	platform_log("Xbox heap: %lu free blocks joined into %lu, %lu MB given back to its top",
		(unsigned long)(count), (unsigned long)kept, (unsigned long)(top >> 20));
	return joined || top;
}

#ifdef HALO_WINDOW_TRANSLATED
/* the bytes carved from the heap so far (its high-water mark: freed blocks
are reused, not returned), for a console's memory report */
size_t xbox_heap_used(void)
{
	return heap_next ? (size_t)(heap_next - (unsigned char *)xbox_pointer(XBOX_HEAP_BASE)) : 0;
}

#endif
BOOL xbox_heap_contains(const void *pointer)
{
#ifdef HALO_WINDOW_TRANSLATED
	unsigned int offset = (unsigned int)(uintptr_t)pointer - xbox_address_space_base;
#else
	unsigned long long offset = (unsigned long long)(uintptr_t)pointer - XBOX_ADDRESS_SPACE_BASE;
#endif

	return offset >= XBOX_HEAP_BASE && offset < XBOX_HEAP_END;
}

void *xbox_heap_allocate(size_t size, BOOL zero)
{
	struct heap_block *block = NULL;
	int index = small_class(size ? size : 1);

	pthread_mutex_lock(&heap_lock);
	if (heap_initialize())
	{
		if (index < SMALL_CLASS_COUNT)
		{
			if (small_free[index])
			{
				block = block_of(small_free[index]);
				small_free[index] = small_free[index]->next;
			}
			else
			{
				block = carve((size_t)16 << index);
			}
		}
		else
		{
			size_t capacity = (size + LARGE_GRANULARITY - 1) & ~(size_t)(LARGE_GRANULARITY - 1);

			block = take_large(capacity);
			if (!block)
				block = carve(capacity);
		}
		/* (AE 0.7.9: out of room: the free blocks joined, then asked again; a small one may take a large free one) */
		if (!block && heap_merge())
		{
			size_t capacity = index < SMALL_CLASS_COUNT ? (size_t)16 << index :
				(size + LARGE_GRANULARITY - 1) & ~(size_t)(LARGE_GRANULARITY - 1);

			if (index < SMALL_CLASS_COUNT && small_free[index])
			{
				block = block_of(small_free[index]);
				small_free[index] = small_free[index]->next;
			}
			if (!block)
				block = take_large(capacity);
			if (!block)
				block = carve(capacity);
		}
	}
	if (block)
		block->free = FALSE;
	pthread_mutex_unlock(&heap_lock);
	if (!block)
	{
		platform_log("Xbox heap exhausted (%lu bytes requested)", (unsigned long)size);
		return NULL;
	}
	if (zero)
		memset(block + 1, 0, size);
	return block + 1;
}

void xbox_heap_free(void *pointer)
{
	struct heap_block *block;
	struct free_node *node = pointer;
	int index;

	if (!pointer)
		return;
	block = block_of(pointer);
	/* (checked under the lock: two threads freeing the same block would
	both see it in use, and list it twice) */
	pthread_mutex_lock(&heap_lock);
	if (block->magic != HEAP_MAGIC || block->free)
	{
		pthread_mutex_unlock(&heap_lock);
		platform_log("Xbox heap: bad free of %p", pointer);
		return;
	}
	index = small_class(block->capacity);
	block->free = TRUE;
	if (index < SMALL_CLASS_COUNT && block->capacity == ((size_t)16 << index))
	{
		node->next = small_free[index];
		small_free[index] = node;
	}
	else
	{
		node->next = large_free;
		large_free = node;
	}
	pthread_mutex_unlock(&heap_lock);
}

size_t xbox_heap_capacity(void *pointer)
{
	return pointer ? (size_t)block_of(pointer)->capacity : 0;
}

#endif /* HALO_64BIT */
