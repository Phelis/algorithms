

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
// support boolean
#include <stdbool.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))


/*
 * Radix tree node definition.
 */
#define RADIX_TREE_MAP_SHIFT  6
#define RADIX_TREE_MAP_SIZE  (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK  (RADIX_TREE_MAP_SIZE-1)


#define RADIX_TREE_INDEX_BITS  (8 /* CHAR_BIT */ * sizeof(unsigned long))
#define RADIX_TREE_MAX_PATH (RADIX_TREE_INDEX_BITS/RADIX_TREE_MAP_SHIFT + 2)

#define RADIX_TREE_INIT()    {                    \
        .height = 0,                            \
        .rnode = NULL,                            \
}

#define RADIX_TREE(name) \
struct radix_tree_root name = RADIX_TREE_INIT()

#define INIT_RADIX_TREE(root)  \
do {                                 \
    (root)->height = 0;                        \
    (root)->rnode = NULL;                        \
} while (0)


// the first !, it makes nonzero become zero, the second ! makes zero become 1.
// expect one
#define likely(x) __builtin_expect(!!(x), 1)
// the first !, it makes nonzero become zero, the second ! makes zero become 1.
// expect zero
#define unlikely(x) __builtin_expect(!!(x), 0)


struct radix_tree_node {
    unsigned int    count;
    void        *slots[RADIX_TREE_MAP_SIZE];
};

struct radix_tree_path {
    struct radix_tree_node *node, **slot;
};

struct radix_tree_root {
    unsigned int            height;
    struct radix_tree_node   *rnode;
};



static unsigned long height_to_maxindex[RADIX_TREE_MAX_PATH];

/*
 * This assumes that the caller has performed appropriate preallocation, and
 * that the caller has pinned this thread of control to the current CPU.
 */
static struct radix_tree_node *
radix_tree_node_alloc(struct radix_tree_root *root)
{
    struct radix_tree_node *ret;
    
    ret = (struct radix_tree_node *)
    calloc (1, sizeof (struct radix_tree_node));
	
    if (!ret)
    	abort();
	
    return ret;
}

/*
 *    Return the maximum key which can be store into a
 *    radix tree with height HEIGHT.
 */
static inline unsigned long radix_tree_maxindex(unsigned int height)
{
    return height_to_maxindex[height];
}

/**
 * Extend a radix tree so it can store key @index.
 */
static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
{
    printf("radix_tree_extend\n");

    struct radix_tree_node *node;
    unsigned int height;
    
    /**
	 * Figure out what the height should be. if the height is not enough,
	 * adds one to the height.
	 */
    height = root->height + 1;
    while (index > radix_tree_maxindex(height)) {
        height++;
    }
    
    if (root->rnode) {
        do {
            if (!(node = radix_tree_node_alloc(root)))
            	return -ENOMEM;
            
            /**
			 * Increase the height by one level above the root
			 */
            node->slots[0] = root->rnode;
            node->count = 1;
            root->rnode = node;
            root->height++;
        } while (height > root->height);
    } else {
        root->height = height;
    }
    
    return 0;
}


/**
 *    radix_tree_insert    -    insert into a radix tree
 *    @root:        radix tree root
 *    @index:        index key
 *    @item:        item to insert
 *
 *    Insert an item into the radix tree at position @index.
 */
int radix_tree_insert(struct radix_tree_root *root, unsigned long index,
                      void *item)
{
    
    printf("radix_tree_insert %lu \n", index);

	// 'node' is represented as the parent node
    struct radix_tree_node *node = NULL, *tmp, **slot;
    unsigned int height, shift;
    int error;
    
    /* Make sure the tree is high enough.  */
    if (index > radix_tree_maxindex(root->height)) {
        error = radix_tree_extend(root, index);
        if (error)
        return error;
    }
	
	// Setup the root, height and shift (shift increased according to height)
    slot = &root->rnode;
    height = root->height;
    shift = (height-1) * RADIX_TREE_MAP_SHIFT;
	
	// height
    while (height > 0) {
		// if the slot or root does not exist,
        if (*slot == NULL) {
            /* Have to add a child node.  */
            if (!(tmp = radix_tree_node_alloc(root)))
            	return -ENOMEM;
            *slot = tmp;
            if (node)
            	node->count++;
        }
        
        /* Go a level down and adopt the slot as a parent node, slot is assigned */
        node = *slot;
        slot = (struct radix_tree_node **)
        		(node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
		
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    
    if (*slot != NULL)
    	return -EEXIST;
	
    if (node)
    	node->count++;
    
    *slot = item;
    return 0;
}


/**
 *    radix_tree_lookup    -    perform lookup operation on a radix tree
 *    @root:        radix tree root
 *    @index:        index key
 *
 *    Lookup them item at the position @index in the radix tree @root.
 */
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
    unsigned int height, shift;
    struct radix_tree_node **slot;
    
    height = root->height;
    if (index > radix_tree_maxindex(height))
    return NULL;
    
    shift = (height-1) * RADIX_TREE_MAP_SHIFT;
    slot = &root->rnode;
    
    while (height > 0) {
        if (*slot == NULL)
        return NULL;
        
        slot = (struct radix_tree_node **)
        ((*slot)->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    
    return (void *) *slot;
}



/**
 *
 */
static unsigned long __maxindex(unsigned int height)
{
    // 每一層會 shift 一遍，所以很多層會 shift 很多遍
    unsigned int tmp = height * RADIX_TREE_MAP_SHIFT;
    unsigned long index = (~0UL >> (RADIX_TREE_INDEX_BITS - tmp - 1)) >> 1;
    
    if (tmp >= RADIX_TREE_INDEX_BITS)
    index = ~0UL;
    
    return index;
}

static void radix_tree_init_maxindex(void)
{
    unsigned int i;
    
    //  總共 12 層，每一層有不一樣的 index
    for (i = 0; i < ARRAY_SIZE(height_to_maxindex); i++) {
        height_to_maxindex[i] = __maxindex(i);
    }
}


void radix_tree_init(void)
{
    radix_tree_init_maxindex();
}

static inline void
radix_tree_node_free(struct radix_tree_node *node)
{
    free (node);
}


/**
 *    radix_tree_delete    -    delete an item from a radix tree
 *    @root:        radix tree root
 *    @index:        index key
 *
 *    Remove the item at @index from the radix tree rooted at @root.
 *
 *    Returns the address of the deleted item, or NULL if it was not present.
 */
void *radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
    printf("radix_tree_delete %lu \n", index);

    struct radix_tree_path path[RADIX_TREE_MAX_PATH], *pathp = path;
    unsigned int height, shift;
    void *ret = NULL;
    
    height = root->height;
    if (index > radix_tree_maxindex(height))
    goto out;
    

    shift = (height-1) * RADIX_TREE_MAP_SHIFT;
    pathp->node = NULL;
    pathp->slot = &root->rnode;
    
    while (height > 0) {
        if (*pathp->slot == NULL)
        goto out;
        
        pathp[1].node = *pathp[0].slot;
        pathp[1].slot = (struct radix_tree_node **)
        (pathp[1].node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK));
        pathp++;
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    
    ret = *pathp[0].slot;
    if (ret == NULL)
    goto out;
    
    *pathp[0].slot = NULL;
    while (pathp[0].node && --pathp[0].node->count == 0) {
        pathp--;
        *pathp[0].slot = NULL;
        radix_tree_node_free(pathp[1].node);
    }
    
    if (root->rnode == NULL)
    root->height = 0;  /* Empty tree, we can reset the height */
out:
    return ret;
}


static /* inline */ unsigned int
__lookup(struct radix_tree_root *root, void **results, unsigned long index,
         unsigned int max_items, unsigned long *next_index)
{
    unsigned int nr_found = 0;
    unsigned int shift;
    unsigned int height = root->height;
    struct radix_tree_node *slot;
    
    shift = (height-1) * RADIX_TREE_MAP_SHIFT;
    slot = root->rnode;
    
    while (height > 0) {
        unsigned long i = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        // 收尋陣列 64 個 elements，如果沒有裝入任何指標，就離開這個 pointer array。 如果有資料
        for ( ; i < RADIX_TREE_MAP_SIZE; i++) {
            if (slot->slots[i] != NULL)
                break;
            
            index &= ~((1 << shift) - 1);
            index += 1 << shift;
            if (index == 0)
                goto out;    /* 32-bit wraparound */
        }
        
        if (i == RADIX_TREE_MAP_SIZE)
            goto out;
        
        height--;
        
        // 當 height 為零的時候，也就是解析到樹葉了
        if (height == 0) {    /* Bottom level: grab some items */
            unsigned long j = index & RADIX_TREE_MAP_MASK;
            
            for ( ; j < RADIX_TREE_MAP_SIZE; j++) {
                index++;
                // 如果 slot 為非零，則就把它收集起來。
                if (slot->slots[j]) {
                    results[nr_found++] = slot->slots[j];
                    if (nr_found == max_items)
                        goto out;
                }
            }
        }
        
        shift -= RADIX_TREE_MAP_SHIFT;
        
        // 往下一個 slot 走過去，
        slot = slot->slots[i];
    }
out:
    *next_index = index;
    return nr_found;
}


unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
                       unsigned long first_index, unsigned int max_items)
{
    const unsigned long max_index = radix_tree_maxindex(root->height);
    unsigned long cur_index = first_index;
    unsigned int ret = 0;
    
    if (root->rnode == NULL)
        goto out;
    
    if (max_index == 0) {            /* Bah.  Special case */
        if (first_index == 0) {
            if (max_items > 0) {
                *results = root->rnode;
                ret = 1;
            }
        }
        goto out;
    }
    
    while (ret < max_items) {
        unsigned int nr_found;
        unsigned long next_index;    /* Index of next search */
        
        if (cur_index > max_index)
        break;
        nr_found = __lookup(root, results + ret, cur_index,
                            max_items - ret, &next_index);
        ret += nr_found;
        if (next_index == 0)
            break;
        cur_index = next_index;
    }
out:
    return ret;
}


/**
 * is little endian
 */
bool is_little_endian() {
    int a = 1;
    return (* ((char *) &a) && 1);
}

struct item {
	unsigned long index;
};


int main (int argc, char** argv) {
    printf("Orginal Memory Size: %lu\n", ARRAY_SIZE(height_to_maxindex));
//
//    if (is_little_endian()) {
//        printf("isLittleEndian %d\n", is_little_endian());
//    }
	
	
	/* Declare and initialize */
	RADIX_TREE(my_tree);
    radix_tree_init();
	
    // insert, index 1 = 123
    radix_tree_insert(&my_tree, 56, "藍色藥水");
    radix_tree_insert(&my_tree, 2, "加速藥水");
    radix_tree_insert(&my_tree, 3, "紅色藥水");
    radix_tree_insert(&my_tree, 63, "慎重藥水");

    char* items[59];
    int nfound;
	
	// Number of items in radix tree
    nfound = radix_tree_gang_lookup(&my_tree, (void **)items ,1, 29);
    printf("nfound:  %d\n", nfound);
	
    // print
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 56));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 2));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 3));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 63));

    // delete
    radix_tree_delete(&my_tree, 56);
    radix_tree_delete(&my_tree, 2);

    // print
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 56));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 2));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 3));
    printf("after insertion: val %s\n", radix_tree_lookup(&my_tree, 63));



    return 0;
}
