AVL树是二叉搜索树的一种，对于AVL树的实现来说，有以下四个核心的点：
- 树结点与树结构的定义；
- 树不平衡时的结构调整（由插入或删除时引起）；
- 插入操作；
- 删除操作


本次学习的目的是**掌握AVL树相对较为优雅的实现方式**。本文的内容是自己学习源码后的总结，会尝试将AVL树实现的四个核心点讲清楚。学习资料来源如下。
- https://github.com/openzfs/zfs
- https://blog.csdn.net/qq_21388535/article/details/105601270


# 一、类型定义
## AVL树结点定义
源码中对于32位和64位不同的系统，提供了不同的树结点定义方式。二者之间的差异通过结构字段的Get和Set宏进行屏蔽，即宏接口是一样的但是针对32位和64位系统下不同结构的定义有不同的实现方式。另注：源码中的另一个技巧是它将树结点与树结构的定义放在了`avl_impl.h`文件之中，并且不允许用户直接引用`avl_impl.h`。用户可以感知的内容在`avl.h`中。用意很明显————用于无需感知的内容需要对用户屏蔽。以上两点代码技巧非常值得在平时写代码时借鉴。

对于64系统，AVL树结点`avl_node`定义如下。对于树结点来说，需要存储左右子树指针、父节点指针、本节点是父节点的左子树还是右子树的标记、平衡系数。

（1）左右子树指针存放在`avl_child`数组中（**这样做的妙用后面再说**）

（2）父节点指针保存在`avl_pcb`的高48bit当中（**可以思考为什么48bit位宽就够保存？**）

（3）左右子树标记占用`avl_pcb`中的第2号bit位

（4）平衡系数分别有-1、0、1三种情况，对其加1后归一成0、1、2，共需要2bit位存储，占用`avl_pcb`的第0和1号bit位


```c
/*
 * for 64 bit machines, avl_pcb contains parent pointer, balance and child_index
 * values packed in the following manner:
 *
 * |63                                  3|        2        |1          0 |
 * |-------------------------------------|-----------------|-------------|
 * |      avl_parent hi order bits       | avl_child_index | avl_balance |
 * |                                     |                 |     + 1     |
 * |-------------------------------------|-----------------|-------------|
 *
 */
struct avl_node {
	struct avl_node *avl_child[2];	/* left/right children nodes */
	uintptr_t avl_pcb;		/* parent, child_index, balance */
};

typedef struct avl_node avl_node_t;
```

可以看到，整个树结点极其精简，节省内存。与此同时，提供了相关字段的Get和Set方法宏，以父节点指针举例。`AVL_XPARENT`用于Get父节点指针，`AVL_SETPARENT`用于Set父节点指针。其他结构也类似，不赘述。额外需要注意平衡系数在Get时需要减1，在Set时需要加1。

```c
#define	AVL_XPARENT(n) \
    ((struct avl_node *)((n)->avl_pcb & ~7))

#define	AVL_SETPARENT(n, p) \
    ((n)->avl_pcb = (((n)->avl_pcb & 7) | (uintptr_t)(p)))
```

```c
#define	AVL_XBALANCE(n) \
    ((int)(((n)->avl_pcb & 3) - 1))

#define	AVL_SETBALANCE(n, b)
    ((n)->avl_pcb = (uintptr_t)((((n)->avl_pcb & ~3) | ((b) + 1))))
```

## AVL树定义
对于一棵AVL树，我们需要存储根结点、结点之间的比较函数（在查找时需要进行比较）。与内核中的`list`类似，用户想使用AVL树接口，需要在其业务结构中嵌入一个`avl_node`用于挂链。不同的是，这里的AVL树结构里包含了`avl_offset`字段，该字段用于存储业务结构体中`avl_node`距离业务结构体首地址的地址偏移量。在`list`中，偏移量是由`CONTAINER_OF`宏来完成的，这里的`avl_offset`的赋值逻辑与`CONTAINER_OF`宏是类似的。`avl_numnodes`则存储了当前AVL树中结点的总个数。
```c
struct avl_tree {
	struct avl_node *avl_root;	/* root node in tree */
	int (*avl_compar)(const void *, const void *);
	size_t avl_offset;		/* offsetof(type, avl_link_t field) */
	ulong_t avl_numnodes;		/* number of nodes in the tree */
#ifndef _KERNEL
	size_t avl_pad;			/* For backwards ABI compatibility. */
#endif
};

typedef struct avl_tree avl_tree_t;
```
注意上述定义中的性能优化技巧。`avl_root`、`avl_compar`和`avl_offset`三个字段对于`avl_find`接口至关重要。而find操作的调用极其频繁，因此我们选择把这三个字段放在结构体最前面，希望它们能够被加载到同一个cache line中，提升cache命中率以提升代码性能。同理对于AVL树结构，也提供了各字段相关的宏接口。`avl_node`转业务结构体需要加上偏移量，反之需要减去偏移量，如下面的两个宏接口所示。
```c
#define	AVL_NODE2DATA(n, o)	((void *)((uintptr_t)(n) - (o)))
#define	AVL_DATA2NODE(d, o)	((struct avl_node *)((uintptr_t)(d) + (o)))
```

# 二、AVL树旋转
当AVL树因为插入新结点或者移除结点造成树不平衡时，需要进行AVL树的旋转调整树结构以保证再次平衡。从课本上我们知道AVL树有个性质就是你只需要找到最小不平衡子树，然后将这棵子树重新调整为平衡之后整棵树就是平衡的。因此可以抽象出下面的接口用于调整最小不平衡子树。`node`指向该子树的根节点，`balance`是根节点插入或删除后的平衡系数（**它只能是2或者-2，想想为什么是这样**）。接口返回值表示最小不平衡子树在调整完成后树高有没有发生变化。
```c
/*
 * Perform a rotation to restore balance at the subtree given by depth.
 *
 * This routine is used by both insertion and deletion. The return value
 * indicates:
 *	 0 : subtree did not change height
 *	!0 : subtree was reduced in height
 *
 * The code is written as if handling left rotations, right rotations are
 * symmetric and handled by swapping values of variables right/left[_heavy]
 *
 * On input balance is the "new" balance at "node". This value is either
 * -2 or +2.
 */
static int
avl_rotation(avl_tree_t *tree, avl_node_t *node, int balance);
```

通过课本我们知道，对于最小不平衡子树来说，插入新结点后（**删除的逻辑不一样，后面再说**）导致不平衡总共有以下四种情况。

（1） LL：新插入的结点在根节点的左子树的左子树上

（2） RR：新插入的结点在根节点的右子树的右子树上

（3）LR：新插入的结点在根结点的左子树的右子树上

（4）RL：新插入的结点在根结点的右子树的左子树上

对于情况（1），我们需要对树进行一次右旋操作

对于情况（2），我们需要对树进行一次左旋操作

对于情况（3），我们需要先对树进行一次左旋再做一次右旋

对于情况（4），我们需要先对树进行一次右旋再做一次左旋

注：以上四种情况的调整方法请参考教科书

可以看到，情况（1）和（2）是对称的，情况（3）和情况（4）也是对称的。为了充分利用对称性，只写一遍代码，我们需要能够灵活处理左右子树的选择。**这就是为什么左右子树被放在了数组中的缘故**。不信请看下面的代码，当`balance`为-2时，`left`值为0，`right`值为1，此时这两个下标指向的是左子树和右子树；当`balance`值为2时，`left`值为1，`right`值为0，此时这两个下标指向的是右子树和左子树。两下标完全调转过来，这意味着此时同样的代码处理的逻辑和之前完全是对称的，左变右，右变左。
```c
int left = !(balance < 0);
int right = 1 - right;
```

注意：源码中结点平衡系数的定义是用右子树的树高减去左子树的树高，这点可能和教科书上的定义相反。因为可以灵活修改下标，我们只需要考虑情况（1）和情况（3）即可。源码中的`avl_rotation`包含了这两种情况，让我们一点一点地拆解函数的实现。

```c
int left = !(balance < 0);	/* when balance = -2, left will be 0 */
int right = 1 - left;
int left_heavy = balance >> 1;
int right_heavy = -left_heavy;
avl_node_t *parent = AVL_XPARENT(node);
avl_node_t *child = node->avl_child[left];
avl_node_t *cright;
avl_node_t *gchild;
avl_node_t *gright;
avl_node_t *gleft;
int which_child = AVL_XCHILD(node);
int child_bal = AVL_XBALANCE(child);
```

`parent`指向的是当前最小不平衡子树根节点的父节点，`child`指向的是根节点的“左”子树（注意`left`值可以为0也可以为1。为0时`child`指向左子树，为1则指向右子树）。`which_child`取的是根节点在父节点的左还是右子树的标记，`child_bal`取的是平衡系数。各变量的变化情况如下所示。
```
balance = -2, left = 0, right = 1, left_heavy = -1, right_heavy = 1
balance =  2, left = 1, right = 0, left_heavy =  1, right_heavy = -1
```

如何区分情况（1）和情况（3）？我们以下图所示的最简单的不平衡子树举例，可以看到`child_bal`和`right_heavy`两变量可以区分LL型和LR型。当`child_bal`不等于`right_heavy`时为LL型，否则为LR型。

[avl0.png](./img/avl0.PNG)

```c
if (child_bal != right_heavy) {
```

```c
	/*
		* compute new balance of nodes
		*
		* If child used to be left heavy (now balanced) we reduced
		* the height of this sub-tree -- used in "return...;" below
		*/
	child_bal += right_heavy; /* adjust towards right */
```
对于LL型，需要做一次右旋。调整后`child`会被提上去，而`node`会被拽下来作为`child`的右子树。由于`child`的右子树高度加1，所以`child`的平衡系数需要加上`right_heavy`值。

```c
	/*
		* move "cright" to be node's left child
		*/
	cright = child->avl_child[right];
	node->avl_child[left] = cright;
	if (cright != NULL) {
		AVL_SETPARENT(cright, node);
		AVL_SETCHILD(cright, left);
	}
```
在`node`被拽下来的同时，`child`的右子树`cright`需要添加到`node`的左子树的位置上。这便是上述6行代码做的事。由于`cright`是作为整体被移动的，所以`cright`的平衡系数不变。只需要在`cright`和`node`之间互相更新父节点与左子树即可。

```c
	/*
		* move node to be child's right child
		*/
	child->avl_child[right] = node;
	AVL_SETBALANCE(node, -child_bal);
	AVL_SETCHILD(node, right);
	AVL_SETPARENT(node, child);
```
随后，将`node`添加到`child`的右子树位置处，这是真正地在执行“将node拽下来”的动作。值得注意的是`node`的平衡系数如何计算？假设在未调整之前`child`的平衡系数为`child_bal`，`node`的平衡系数为`balance`，则有以下等式成立。各变量含义为：

- `h(x)`：树x的高度
- `nright`：`node`的右子树
- `nleft`：`node`的左子树
- `cright`：`child`的右子树
- `cleft`：`child`的左子树


```
child_bal = h(cright) - h(cleft)

balance 
= h(nright) - h(child)
= h(nright) - (h(cleft) + 1) //对于LL型来说，child的树高为左子树树高加上1
= h(nright) - h(cleft) - 1

h(nright) - h(cright)        //这个是调整后的node的平衡系数
= balance + h(cleft) + 1 - （child_bal + h(cleft))
= balance + 1 - child_bal    //对于LL型来说，balance为-2
= -1 - child_bal
= -(child_bal + 1)
= -(child_bal + right_heavy) //对于LL型来说，right_heavy为1
```
由于`child_bal`已经在上面的步骤中已经加上了`right_heavy`，所以调整之后的`node`的平衡系数为`-child_bal`。推导完毕

```c
	/*
		* update the pointer into this subtree
		*/
	AVL_SETBALANCE(child, child_bal);
	AVL_SETCHILD(child, which_child);
	AVL_SETPARENT(child, parent);
	if (parent != NULL)
		parent->avl_child[which_child] = child;
	else
		tree->avl_root = child;

	return (child_bal == 0);
}
```
随后，设置`child`的新平衡系数为`child_bal`，同时将其在父节点中的标记改为`which_child`，即`child`已经顶替了原先`node`的位置了，同时将`child`的父节点设置为原先`node`的父节点。如果父节点不为空，还需要将父节点的孩子设置为`child`。特别地，如果父节点为空则需要将根节点设置为`child`。最后，LL型调整后，子树树高是一定会降低的。所以`child_bal == 0`是必成立的（**有例外情况吗？存疑**），函数返回非0值。至此，LL型不平衡子树调整完毕。

如下图所示，对于LR型，涉及到调整的几个树结点只有`child`的右子树`gchild`，以及`gchild`的左右子树`gleft`和`gright`。通过和调整后的图形对比可以发现，`gchild`会顶替原先`node`的位置。同时`node`被拽了下来，成了`gchild`的右子树。而`gleft`成了`child`的右子树；`gright`成了`node`的左子树。

[avl1.PNG](./img/avl0.PNG)

```c
gchild = child->avl_child[right];
gleft = gchild->avl_child[left];
gright = gchild->avl_child[right];
```
为了方便，先将涉及到调整的树结点给取出来。

```c
/*
* move gright to left child of node and
*
* move gleft to right child of node
*/
node->avl_child[left] = gright;
if (gright != NULL) {
	AVL_SETPARENT(gright, node);
	AVL_SETCHILD(gright, left);
}

child->avl_child[right] = gleft;
if (gleft != NULL) {
	AVL_SETPARENT(gleft, child);
	AVL_SETCHILD(gleft, right);
}
```
紧接着就将`gright`放到了`node`的左子树的位置，将`gleft`放到了`child`的右子树位置。

```c
/*
* move child to left child of gchild and
*
* move node to right child of gchild and
*
* fixup parent of all this to point to gchild
*/
balance = AVL_XBALANCE(gchild);
gchild->avl_child[left] = child;
AVL_SETBALANCE(child, (balance == right_heavy ? left_heavy : 0));
AVL_SETPARENT(child, gchild);
AVL_SETCHILD(child, left);

gchild->avl_child[right] = node;
AVL_SETBALANCE(node, (balance == left_heavy ? right_heavy : 0));
AVL_SETPARENT(node, gchild);
AVL_SETCHILD(node, right);

AVL_SETBALANCE(gchild, 0);
AVL_SETPARENT(gchild, parent);
AVL_SETCHILD(gchild, which_child);
if (parent != NULL)
	parent->avl_child[which_child] = gchild;
else
	tree->avl_root = gchild;

return (1);	/* the new tree is always shorter */
```
随后，将`child`设置为`gchild`的左子树，将`node`设置为`gchild`的右子树。同理，由于`gchild`顶替了原先`node`的位置，所以需要将`gchild`的父节点设置为`node`的父结点，将`gchild`设置为`node`父结点的孩子。如果`node`的父结点为空，则需要将AVL树根节点更新为`gchild`。我们最后来讨论调整后，`child`、`node`和`gchild`的平衡系数的计算问题。

易知新结点插入后，`gchild`的平衡系数`gchild_bal`只可能为1或者-1。因为若`gchild_bal`平衡系数变为0，说明子树`gchild`的树高在新结点插入后不变。于是`child`和`node`结点的平衡系数不变，这与`node`为最小不平衡子树（LR型的最小不平衡子树的平衡系数为-2）矛盾。举一个特例如下图所示。

[avl2.PNG](./img/avl2.PNG)

由上图易知有如下关系成立，而`gchild_bal`平衡系数调整后均为0。
```
 IF gchild_bal == 1 == right_heavy
 THEN
	child_bal == -1 == left_heavy
	node_bal == 0
END

IF gchild_bal == -1 == left_heavy
THEN
	child_bal == 0
	node_bal == 1 == right_heavy
END

gchild_bal = 0 [After Adjustment]
```

翻译成代码就是：
```c
AVL_SETBALANCE(child, (balance == right_heavy ? left_heavy : 0));
AVL_SETBALANCE(node, (balance == left_heavy ? right_heavy : 0));
AVL_SETBALANCE(gchild, 0);
```

至此，AVL树的旋转操作就全部讲完了。在LL型调整处有一个问题存疑，在LR调整处则是举了一个特例来说明三个结点调整后的平衡系数的计算，缺少严格的证明。

# 三、AVL树初始化与查找
与内核中的`list`类似，AVL树只负责组织数据并提供相关操作接口，内存管理不是AVL树的职责所在。因此AVL树的初始化只涉及字段的赋值。

```c
void
avl_create(avl_tree_t *tree, int (*compar) (const void *, const void *),
    size_t size, size_t offset)
{
	tree->avl_compar = compar;
	tree->avl_root = NULL;
	tree->avl_numnodes = 0;
	tree->avl_offset = offset;
}
```

AVL树的搜索和二叉搜索树是一样的流程。注意在搜索过程中调用`avl_compar`时的入参类型是业务结构体指针，所以要求用户提供的比较函数的两个入参也应该是业务结构体的指针。注意，如果有相同的value，那么where返回的位置正好指向原先那个相同value值的位置，于是原先的value会在后续的插入过程中被覆盖。
```c
/*
 * Search for the node which contains "value".  The algorithm is a
 * simple binary tree search.
 *
 * return value:
 *	NULL: the value is not in the AVL tree
 *		*where (if not NULL)  is set to indicate the insertion point
 *	"void *"  of the found tree node
 */
void *
avl_find(avl_tree_t *tree, const void *value, avl_index_t *where)
{
	avl_node_t *node;
	avl_node_t *prev = NULL;
	int child = 0;
	int diff;
	size_t off = tree->avl_offset;

	for (node = tree->avl_root; node != NULL;
	    node = node->avl_child[child]) {

		prev = node;

		diff = tree->avl_compar(value, AVL_NODE2DATA(node, off));
		if (diff == 0) {
			return (AVL_NODE2DATA(node, off));
		}
		child = (diff > 0);
	}

	if (where != NULL)
		*where = AVL_MKINDEX(prev, child);

	return (NULL);
}
```

# 四、AVL树插入结点
AVL树的插入过程首先是调用`avl_find`查找value应该插入的位置。随后将value插入该位置后不断往父结点方向遍历找到最小不平衡子树（如果存在的话），最后调用`avl_rotate`重新将树调整为平衡状态。
```c
/*
 * Insert a new node into an AVL tree at the specified (from avl_find()) place.
 *
 * Newly inserted nodes are always leaf nodes in the tree, since avl_find()
 * searches out to the leaf positions.  The avl_index_t indicates the node
 * which will be the parent of the new node.
 *
 * After the node is inserted, a single rotation further up the tree may
 * be necessary to maintain an acceptable AVL balance.
 */
void
avl_insert(avl_tree_t *tree, void *new_data, avl_index_t where)
{
	avl_node_t *node;
	avl_node_t *parent = AVL_INDEX2NODE(where);
	int old_balance;
	int new_balance;
	int which_child = AVL_INDEX2CHILD(where);
	size_t off = tree->avl_offset;
```
代码首先根据传入的where解析出待插入位置的父结点`parent`和左右子树标记`which_child`。

```c
	node = AVL_DATA2NODE(new_data, off);

	/*
	 * First, add the node to the tree at the indicated position.
	 */
	++tree->avl_numnodes;

	node->avl_child[0] = NULL;
	node->avl_child[1] = NULL;

	AVL_SETCHILD(node, which_child);
	AVL_SETBALANCE(node, 0);
	AVL_SETPARENT(node, parent);
	if (parent != NULL) {
		// ASERT(parent->avl_child[which_child] == NULL);
		parent->avl_child[which_child] = node;
	} else {
		// ASERT(tree->avl_root == NULL);
		tree->avl_root = node;
	}
```
随后进行插入操作。由于新插入节点为叶子节点，所以左右子树设为NULL，同时总的结点数加1；最后设置新插入结点和父结点之间的关系。

```c
	/*
	 * Now, back up the tree modifying the balance of all nodes above the
	 * insertion point. If we get to a highly unbalanced ancestor, we
	 * need to do a rotation.  If we back out of the tree we are done.
	 * If we brought any subtree into perfect balance (0), we are also done.
	 */
	for (;;) {
		node = parent;
		if (node == NULL)
			return;

		/*
		 * Compute the new balance
		 */
		old_balance = AVL_XBALANCE(node);
		new_balance = old_balance + (which_child ? 1 : -1);

		/*
		 * If we introduced equal balance, then we are done immediately
		 */
		if (new_balance == 0) {
			AVL_SETBALANCE(node, 0);
			return;
		}

		/*
		 * If both old and new are not zero we went
		 * from -1 to -2 balance, do a rotation.
		 */
		if (old_balance != 0)
			break;

		AVL_SETBALANCE(node, new_balance);
		parent = AVL_XPARENT(node);
		which_child = AVL_XCHILD(node);
	}
	/*
	 * perform a rotation to fix the tree and return
	 */
	(void) avl_rotation(tree, node, new_balance);
}
```
最后的步骤是寻找最小不平衡子树并调整之。因为新插入结点会影响其父结点的平衡系数，所以迭代起点`node`从父结点开始。首先保存`node`的旧的平衡系数`old_balance`，然后再根据新插入结点是父结点的左子树还是右子树决定新的平衡系数`new_balance`是在`old_balance`的基础上加1还是减1。

（1）如果`new_balance`变为0了，那说明新结点的插入没有造成子树`node`的树高度变化，同时其也是平衡的。此时，无需进行任何调整直接返回即可。

（2）如果`old_balance`不为0，那么`new_balance`必为2或者-2，此时`node`这棵子树就是我们要找的最小不平衡子树，退出循环调用`avl_rotate`即可

如果上述情况均不成立，那么我们需要继续往父结点的方向往上遍历寻找。在向上遍历之前把`node`的平衡系数置为`new_balance`。然后设置`node`为其父结点，重复上述过程。最后的结果要么是遍历到了某个父结点之后，发现新插入结点没有造成该父结点的树高度变化（情况1），或者是该父结点不平衡（情况2）；要么是遍历到最后都没有发现最小不平衡子树，即整棵树仍然是平衡的。此时`node`为NULL，无需调整函数直接返回。

# 五、AVL树移除结点
和插入一样，移除结点也可能造成AVL树的不平衡，同样需要调用`avl_rotate`进行调整。根据参考文献【】可以知道，待移除的结点只能是以下四种情况之一。

（1）叶子结点：直接移除该结点，然后向上遍历找到最小不平衡子树并进行调整

（2）仅有左孩子的非叶子结点：将该结点的值和左孩子的值交换，随后移除其左孩子结点。此时移除的左孩子结点是叶子结点

（3）仅有右孩子的非叶子结点：将该结点的值和右孩子的值交换，随后移除其右孩子结点。此时移除的右孩子结点时叶子结点（和情况2是对称的）

（4）含有左右孩子的非叶子结点：将该结点的值与其前驱（或者后继）结点的值交换，随后移除其前驱（或者后继）节点。同理待移除的结点必定为叶子结点

可以看到，无论哪种情况最终都会转换成对叶子结点的移除。那么，要如何将上述四种情况给归一到一个`avl_remove`接口里处理呢？我们可以先处理情况（4），随后处理情况（2）和（3），最后再处理情况（1），这几种情况是递进的。

```c
void
avl_remove(avl_tree_t *tree, void *data)
{
	avl_node_t *del;
	avl_node_t *parent;
	avl_node_t *node;
	avl_node_t tmp;
	int old_balance;
	int new_balance;
	int left;
	int right;
	int which_child;
	size_t off = tree->avl_offset;

	del = AVL_DATA2NODE(data, off);
```
首先取得要待移除的业务结点里包含的avl结点指针。

```c
	/*
	 * Deletion is easiest with a node that has at most 1 child.
	 * We swap a node with 2 children with a sequentially valued
	 * neighbor node. That node will have at most 1 child. Note this
	 * has no effect on the ordering of the remaining nodes.
	 *
	 * As an optimization, we choose the greater neighbor if the tree
	 * is right heavy, otherwise the left neighbor. This reduces the
	 * number of rotations needed.
	 */
	if (del->avl_child[0] != NULL && del->avl_child[1] != NULL) {

		/*
		 * choose node to swap from whichever side is taller
		 */
		old_balance = AVL_XBALANCE(del);
		left = (old_balance > 0);
		right = 1 - left;

		/*
		 * get to the previous value'd node
		 * (down 1 left, as far as possible right)
		 */
		for (node = del->avl_child[left];
		    node->avl_child[right] != NULL;
		    node = node->avl_child[right])
			;
```
首先判断是否属于情况（4），即待移除结点是含有左右子树的非叶子结点。根据结点的平衡系数选择前驱还是后继，即哪边子树更高就选择哪边。随后通过for循环去寻找该前驱（或者后继，下同，不再赘述）结点。上述代码运行完毕后，`node`就指向了前驱结点。

```c
		/*
		 * create a temp placeholder for 'node'
		 * move 'node' to delete's spot in the tree
		 */
		tmp = *node;

		*node = *del;
		if (node->avl_child[left] == node)
			node->avl_child[left] = &tmp;

		parent = AVL_XPARENT(node);
		if (parent != NULL)
			parent->avl_child[AVL_XCHILD(node)] = node;
		else
			tree->avl_root = node;
		AVL_SETPARENT(node->avl_child[left], node);
		AVL_SETPARENT(node->avl_child[right], node);
```
接下来是最为晦涩难懂的一段代码了。首先将`node`结点的内容给复制了一份存于`tmp`中，随后将`del`结点的内容拷贝到`node`所指向结点中，注意这不会改变`node`的指向，但是对`node`所指向那块内存的操作就相当于是在操作`del`结点（**仔细理解一下**）。

然后判断待删除结点的左子树（`node->avl_child[left]`）是否就是其前驱结点`node`。若是则修改待删结点的左子树为临时拷贝的`tmp`结点。

获取待删除结点的父结点（`AVL_XPARENT(node)`），若该父结点不为空，则将其孩子结点赋值为待删除结点的前驱结点`node`。若为空，则更新树的根结点。同时将待删结点的左右子树的父结点设置为当前的前驱结点`node`。这整个过程相当于是用前驱结点替代了待删除结点。

```c
		/*
		 * Put tmp where node used to be (just temporary).
		 * It always has a parent and at most 1 child.
		 */
		del = &tmp;
		parent = AVL_XPARENT(del);
		parent->avl_child[AVL_XCHILD(del)] = del;
		which_child = (del->avl_child[1] != 0);
		if (del->avl_child[which_child] != NULL)
			AVL_SETPARENT(del->avl_child[which_child], del);
	}
```
前驱结点顶替了待删除结点的位置后，之前保存的前驱结点`tmp`就成了真正的要被移除的结点。并且其含有一个或者零个孩子。以上这几行代码的作用是用`tmp`结点彻底顶替原先的前驱结点，向上`tmp`结点要挂在新的父结点下面，向下要接管原先前驱结点的孩子结点。

```c
	/*
	 * Here we know "delete" is at least partially a leaf node. It can
	 * be easily removed from the tree.
	 */
	// ASERT(tree->avl_numnodes > 0);
	--tree->avl_numnodes;
	parent = AVL_XPARENT(del);
	which_child = AVL_XCHILD(del);
	if (del->avl_child[0] != NULL)
		node = del->avl_child[0];
	else
		node = del->avl_child[1];

	/*
	 * Connect parent directly to node (leaving out delete).
	 */
	if (node != NULL) {
		AVL_SETPARENT(node, parent);
		AVL_SETCHILD(node, which_child);
	}
	if (parent == NULL) {
		tree->avl_root = node;
		return;
	}
	parent->avl_child[which_child] = node;
```
接下来要做的就是移除这个`del`结点，将树结点总数自减然后获取其父结点与左右孩子标记，并用`node`指向其不空的那个孩子结点。若`node`不为空，则需要用`node`顶替`del`结点的位置，即设置`node`的父结点与左右孩子标记。同理，若父结点`parent`为空，需要更新树结点为`node`。实际上，上述代码不仅处理了情况（2）和（3），也同时处理了情况（1）。做完上述步骤，`del`结点就从整棵树中给移除出去了。

```c
	do {

		/*
		 * Move up the tree and adjust the balance
		 *
		 * Capture the parent and which_child values for the next
		 * iteration before any rotations occur.
		 */
		node = parent;
		old_balance = AVL_XBALANCE(node);
		new_balance = old_balance - (which_child ? 1 : -1);
		parent = AVL_XPARENT(node);
		which_child = AVL_XCHILD(node);

		/*
		 * If a node was in perfect balance but isn't anymore then
		 * we can stop, since the height didn't change above this point
		 * due to a deletion.
		 */
		if (old_balance == 0) {
			AVL_SETBALANCE(node, new_balance);
			break;
		}

		/*
		 * If the new balance is zero, we don't need to rotate
		 * else
		 * need a rotation to fix the balance.
		 * If the rotation doesn't change the height
		 * of the sub-tree we have finished adjusting.
		 */
		if (new_balance == 0)
			AVL_SETBALANCE(node, new_balance);
		else if (!avl_rotation(tree, node, new_balance))
			break;
	} while (parent != NULL);
}
```
从`parent`结点开始，检查是否需要调整AVL树。若原先`parent`的平衡系数为0，则移除结点后子树的树高不变且`parent`是平衡的，此时无需调整直接返回即可。若移除结点后`parent`的新平衡系数为0，则对于`parent`来说它是平衡的，此时需要继续向上遍历，查看`parent`的父结点是否平衡。否则，当前的`node`结点必定是最小不平衡子树（**想想这是为什么**）。调整之，若调整之后的树高没有发生变化，则直接退出即可。否则同样需要继续向上遍历。至此，整个remove过程就讲清楚了。






