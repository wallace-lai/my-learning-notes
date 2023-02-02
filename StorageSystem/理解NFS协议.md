本次的学习目的是**详细了解NFSv3协议**，绝不能泛泛而谈，否则解决不了工作中遇到的问题。主要参考资料有以下这些。

- NFS: Network File System Protocol Specification


- NFSv3 RFC Specification


- 《操作系统导论》第48章


- [NFS各个版本之间的比较](https://www.cnblogs.com/tcicy/p/9328990.html#:~:text=NFS%E6%98%AF%E4%B8%80%E7%A7%8D%E7%BD%91%E7%BB%9C,NFSv4.1%E3%80%82)

## 一、NFS协议介绍
《操作系统导论》第48章给出了关于NFS协议的大体介绍，内容非常棒。关于其中的要点可以做如下的总结。

网络文件系统NFS属于分布式文件系统的大范畴，分布式文件系统的大体框架如下所示。NFS可以看成是一个特定的分布式文件系统，自然也是遵循下图的体系结构的。大概原理是客户端文件系统对上承接应用程序的POSIX文件系统接口，将其转换成标准的NFS消息发送给文件服务器。文件服务器根据收到的NFS消息做相应处理后将处理结果发送给客户端文件系统。核心的客户端文件系统在这里起到了两个核心作用：

（1）对上层提供POSIX语义实现，后面会讲到NFS客户端和服务器之间的交互被设计成无状态的，因此在客户端这里需要将其于服务器之间的交互包装成有状态的POSIX接口供应用程序使用

（2）将应用程序的POSIX语义的文件操作转换成NFS协议消息发送给服务端，随后将服务端的回复消息转换后提供给应用程序。

[nfs0.png](./img/nfs0.PNG)

针对分布式环境，NFSv2的设计目标可以概括成以下三个关键字。我们分别做一些总结性阐述，详细内容请参考原文《操作系统导论》。

（1）协议无状态

（2）操作幂等性

（3）缓存与缓存一致性问题

## 协议无状态
考虑到分布式环境中随时可能有服务器奔溃，需要让服务器在奔溃后立即恢复。而立即恢复的关键在于客户端和服务端之间的交互是无状态的，服务器不会记录客户端上发生的任何事情，客户端的每次请求都需要给出完成请求所需要的全部信息。不这样做会有什么后果？试想客户端在首次read调用后从服务端拿到了文件描述符，在客户端的第二次read调用之间服务端奔溃将保存在内存中的文件描述符信息丢失了，随后服务端进程重启。随后客户端拿着旧的文件描述符发来第二次read调用，请问此时服务端怎样才能知道这个旧的文件描述符对应哪个文件？

为了解决这个问题，**NFS协议的设计者们采用了无状态的方式。客户端的每次请求都需要携带完成该请求的所有信息**，这样服务端的奔溃恢复逻辑可以做的很简单，可能只需要重启进程即可。但这样做的代价是增大了每次操作所需的网络流量。

## 操作幂等性
何为幂等性？幂等性指的是操作执行一次和执行多次的效果是一样的。NFS协议是具有幂等性的，为什么要这么设计？还是为了让NFS协议在分布式系统中能够更好地处理服务器故障。当客户端向服务器发送消息时，有时会收不到回复。这个时候客户端应该怎么做？很简单直接重试就好，既然所有操作都是幂等性的，那就多做一次呗。你看，操作具有幂等性后，故障处理就变得简单优雅了。

## 缓存与缓存一致性
为了让性能不变得过于糟糕，NFS客户端是有文件系统缓存的。然而，这会引入缓存一致性的问题。

想象一下客户端C1读取文件F，并将文件的副本保存在其本地缓存中。现在假设一个不同的客户端C2覆盖文件F，从而改变其内容。我们称该文件的新版本为F（版本2），或`F[v2]`，称旧版本为`F[v1]`，以便区分两者。最后，还有第三个客户端C3，尚未访问文件F。

第一个问题，假设在客户端C2将它的写入缓存一段时间期间，客户端C3对F的读取将得到的是旧版本的数据`F[v1]`。这个一致性问题被称为“更新可见性问题”，即来自一个客户端的更新什么时候会被其他客户端看到

第二个问题，假设客户端C2将它的写入缓存进行了刷盘，即文件服务器上是`F[v2]`的新版本。但是，之前先于C2读取的C1客户端上的缓存仍然是旧的`F[v1]`版本。这个一致性问题被称为“陈旧缓存”问题。

【nfs1.PNG】

为了解决上面的问题，NFS提出了两种方法。其一是关闭时刷新，即应用程序写入文件并随后关闭文件时，客户端会将文件的所有缓存刷新到服务器上。该方法能够保证后续从另一个节点打开文件时，可以看到最新的文件版本；其二是客户端在使用缓存内容会首先检查文件是否被更改。如果文件的修改时间晚于文件被提取到服务器上的时间则缓存失效，客户端需要重新从服务器上读取文件的最新版本内容并放入缓存中。结合方法一的保证，方法二可以解决陈旧缓存问题。

以上两个方法虽然能够解决缓存一致性问题，但是**性能、性能、还是性能**！。如何在解决缓存一致性的基础上尽可能提升分布式文件系统的性能一直是存储厂商需要重点解决的问题。性能问题一直属于存储系统中核心的Big Problem，后续有时间一定要梳理一番。

总结：个人觉得《操作系统导论》第48章写得非常棒，基本上把NFS协议介绍、NFS协议设计思路、NFS协议实现过程中需要解决的核心的Big Problem给讲清楚了。后续可以结合一些开源的NFS实现去审视上面的设计思路，相信可以收获更深刻的理解。

## 二、NFS协议特性
### 2.1 搭建与配置NFS服务
（1）普通的Linux主机就能充当NFS客户端，但是要让普通Linux具备NFS服务器的功能则需要额外安装软件。在Ubuntu上可以安装`nfs-kernel-server`。
```
sudo apt-get install nfs-kernel-server
```

（2）设置NFS共享目录供NFS客户端挂载
```
mkdir /home/yourname/SHARE
```

（3）配置NFS共享目录
打开`/etc/exports`配置文件，在最后加入以下配置项
```
/home/yourname/SHARE *(rw,sync,no_root_squash,no_subtree_check)
```
【可以研究一下这些配置项的含义】

（4）重启NFS服务
```
sudo  systemctl restart nfs-kernel-server.service
```

（5）客户端挂载共享目录
```
sudo mount -t nfs -o vers=3 xxx.xxx.xxx.xxx:/shared/path /mount/path
```
`xxx.xxx.xxx.xxx`是NFS服务器的ip地址，后面接服务器上的共享目录，最后是共享目将要在客户端上挂载的目录

### 2.2 NFSv2协议
**1. INTRODUCTION**
> The NFS protocol is designed to be portable across different machines, operating systems, network architectures, and transport protocols.

设计NFS协议的目的很简单————在不同机器类型、不同操作系统、不同网络架构之间实现文件共享

> A goal of NFS was to not require any specific level of reliability from its lower levels, so it could potentially be used on many underlying transport protocols, or even another remote procedure call implementation. For ease of discussion, the rest of this document will assume NFS is implemented on top of Sun RPC, described in RFC 1057

NFS协议的设计目标之一就是不依赖任何底层网络的可靠性，为了达到这个目的，标准NFS协议构建在RPC标准协议（由RFC 1057描述）之上。注：**NFSv2协议的RFC于1989年发布，当时基于的RPC协议由RFC-1057描述，但是后续的RPC协议描述文档有RFC-1050、RFC-1057、RFC-1831**。

> The eXternal Data Representation (XDR) standard provides a common way of representing a set of data types over a network. The NFS Protocol Specification is written using the RPC data description language.

XDR标准提供了在网络传输中表示数据类型的通用方法，NFS协议将以RPC数据描述语言来表述。

> The NFS protocol was intended to be as stateless as possible. That is, a server should not need to maintain any protocol state information about any of its clients in order to function correctly.

NFS协议被设计成无状态的，这样做的目的在上面的章节中已经阐述过了，不再赘述。

**2. NFS PROTOCOL DEFINITION**
> The protocol definition is given as a set of procedures with arguments and results defined using the RPC language. A brief description of the function of each procedure should provide enough information to allow implementation.

完整NFS协议的描述由一组用RPC语言表达的接口程序（procedures）给出。简要描述上述接口足以指导协议的实现。

> All of the procedures in the NFS protocol are assumed to be synchronous. When a procedure returns to the client, the client can assume that the operation has completed and any data associated with the request is now on stable storage.

NFS协议中的所有接口都应该是同步的。NFSv2中所有的接口情况如下所示，只要稍微有点文件系统经验的人都能明白这些操作的含义。

```c
// 空操作
void
NFSPROC_NULL(void) = 0;

// 获取文件的attr
attrstat
NFSPROC_GETATTR(fhandle) = 1;

// 设置文件attr
struct sattrargs {
    fhandle file;
    sattr attributes;
}

attrstat
NFSPROC_SETATTR(sattrargs) = 2;

// 已被废弃
void
NFSPROC_ROOT(void) = 3;

diropres
NFSPROC_LOOKUP(diropargs) = 4;

readlinkres
NFSPROC_READLINK(fhandle) = 5;

readres
NFSPROC_READ(readargs) = 6;

void
NFSPROC_WRITECACHE(void) = 7;

attrstat
NFSPROC_WRITE(writeargs) = 8;

diropres
NFSPROC_CREATE(createargs) = 9;

stat
NFSPROC_REMOVE(diropargs) = 10;

stat
NFSPROC_RENAME(renameargs) = 11;

stat
NFSPROC_LINK(linkargs) = 12;

stat
NFSPROC_SYMLINK(symlinkargs) = 13;

diropres
NFSPROC_MKDIR(createargs) = 14;

stat
NFSPROC_RMDIR(diropargs) = 15;

readdirres
NFSPROC_READDIR(readdirargs) = 16;

statfsres
NFSPROC_STATFS(fhandle) = 17;
```

**Basic Data Types**

```c
enum stat {
    NFS_OK = 0,
    NFSERR_PERM = 1,
    NFSERR_NOENT = 2,
    NFSERR_IO = 5,
    NFSERR_NXIO = 6,            // No such device or address
    NFSERR_ACCES = 13,          // Permission denied
    NFSERR_EXIST = 17,
    NFSERR_NODEV = 19,          // No such devie
    NFSERR_NOTDIR = 20,
    NFSERR_ISDIR = 21,
    NFSERR_FBIG = 27,           // File too large
    NFSERR_NOSPC = 28,
    NFSERR_ROFS = 30,
    NFSERR_NAMETOOLONG = 63,
    NFSERR_NOTEMPTY = 66,
    NFSERR_DQUOT = 69,          // Disk quota exceeded
    NFSERR_STALE = 70,          // The fhandle given in the arguments was invalid
    NFSERR_WFLUSH = 99
};
```
stat是每个接口的返回状态集合

```c
enum ftype {
    NFNON = 0,  // a non-file
    NFREG = 1,  // regular file
    NFDIR = 2,  // directory
    NFBLK = 3,  // block-special device
    NFCHR = 4,  // character-special device
    NFLNK = 5   // symbolic link
};
```
ftype表示文件类型

```c
typedef opaque fhandle[FHSIZE];
```
> The "fhandle" is the file handle passed between the server and the client. All file operations are done using file handles to refer to a file or directory. The file handle can contain whatever information the server needs to distinguish an individual file.

fhandle指的是文件句柄，用于在客户端和服务端之间标识唯一的文件。我理解因为NFS是无状态的协议，所以fhandle里必须保存能够找到该文件所需的全量信息（比如：文件系统ID + 文件路径之类的）。即使是fhandle因为服务器奔溃丢失了也没关系，客户端再lookup一下将fhandle生成就行了。

```c
struct timeval {
    unsigned int seconds;
    unsigned int useconds;
};
```
timeval用于传递时间信息，它表示从1970年1月1日零时起经过的秒和毫秒（是毫秒还是纳秒？）数

```c
struct fattr {
    ftype        type;
    unsigned int mode;
    unsigned int nlink;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int blocksize;
    unsigned int rdev;
    unsigned int blocks;
    unsigned int fsid;
    unsigned int fileid;
    timeval      atime;
    timeval      mtime;
    timeval      ctime;
};
```
fattr结构体用于存储文件属性，对此我有个疑问——扩展属性xattr在哪里？或许是因为NFSv2提出的年代太早了，那时候的文件系统根本不支持文件扩展属性吧

```c
struct sattr {
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    timeval      atime;
    timeval      mtime;
};
```
sattr表示客户端允许设置的文件属性字段，字段含义和fattr中字段含义相同

```c
typedef string filename<MAXNAMELEN>;
```
filename用于传递文件名

```c
typedef string path<MAXPATHLEN>;
```
path表示文件路径

```c
union attrstat switch (stat status) {
    case NFS_OK:
        fattr attributes;
    default:
        void;
};
```

```c
struct diropargs {
    fhandle dir;
    filename name;
};
```

```c
union diropres switch (stat status) {
    case NFS_OK:
        struct {
            fhandle file;
            fattr attributes;
        } diropok;
    default:
        void;
};
```

**3. NFS IMPLEMENTATION ISSUE**



