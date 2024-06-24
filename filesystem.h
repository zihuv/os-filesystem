#include <stdio.h>
#include <string.h>
// 定义文件数据结构，全局变量

// 包含：“.” + “..” + 14 个普通文件
// -----内存-----
// 以内存的方式映射整个磁盘文件，指向内存首地址
char *file_disk;
char *curPath;

// -----磁盘-----
// 磁盘文件名称
#define DISK_NAME "disk"
// 数据块总共个数
#define BLOCK_TOTAL_NUM 256
// 单个数据块占用空间，单位字节
#define BLOCK_SIZE 4096
// 所有数据块占用空间，即 数据块个数 * 占用空间
#define BLOCK_TOTAL_SIZE (BLOCK_TOTAL_NUM * BLOCK_SIZE)
// 直接指针数。一个 inode 对应一个文件，一个文件可能占用多个数据块
#define DIRECT_POINTERS_COUNT 12
// 最大文件名称长度
#define MAX_NAME_LENGTH 13

// -----枚举-----
int TYPE_FILE = 0;
int TYPE_DIRECTORY = 1;
char *ROOT_PATH = "root";

// 超级块
typedef struct
{
    int totalBlocks;       // 文件系统总块数
    int blockSize;         // 块大小
    int inodeCount;        // 总inode数
    int freeInodes;        // 空闲 inode 数
    int inodeSize;         // inode 大小
    int inodeOffset;       // inode 偏移量。实际偏移量 = 偏移量 * blockSize，下同
    int dataOffset;        // 数据块偏移量
    int inodeBitmapOffset; // inode 位图偏移量
    int dataBitmapOffset;  // 数据块位图偏移量
} Superblock;
Superblock *superblock;

// 索引节点位图
typedef struct
{
    int bitmap[32];
} InodeBitmap;
InodeBitmap *inodeBitmap;

// 数据区位图
typedef struct
{
    int bitmap[32];
} DataBitmap;
DataBitmap *dataBitmap;

// 索引节点
typedef struct
{
    int no;                               // 编号
    char name[MAX_NAME_LENGTH];           // 文件名
    int type;                             // 文件类型。0-文件，1-目录
    int size;                             // 文件大小，单位字节
    int blockOffset;                      // 块内偏移量，在间接指针情况下使用
    time_t createdAt;                     // 创建时间
    time_t lastAccessedAt;                // 最后访问时间
    time_t lastModifiedAt;                // 最后修改时间
    int directPtr[DIRECT_POINTERS_COUNT]; // 直接指针。初始化“-1”表示没有指向任何东西。作为目录时，指向当前目录文件的 inode 偏移量（不能直接存地址）；作为小文件时，指向数据区
    int indirectPtr;                      // 间接指针。创建大文件时，指向索引文件
    int parentPtr;                        // 指向父级目录。创建上级目录和当前目录时，分别指向上级目录和当前目录的 inode
} Inode;
Inode *firstInode; // inode 起始地址，firstInode + no
Inode *rootInode;  // 根目录
Inode *curInode;   // 当前目录

// 数据区
typedef struct
{
    char data[BLOCK_SIZE];
} DataRegion;
DataRegion *firstDataRegion;

void initSuperblock()
{
    superblock = (Superblock *)file_disk;
    superblock->totalBlocks = BLOCK_TOTAL_NUM;
    superblock->blockSize = BLOCK_SIZE;
    superblock->inodeSize = 256; // 256B 的 inode
    superblock->inodeCount = 256;
    superblock->freeInodes = superblock->inodeCount;
    superblock->inodeOffset = 3; // 第 3 个数据块是 inode
    superblock->dataOffset = 19; // 第 19 个数据块是数据区
    superblock->inodeBitmapOffset = 1;
    superblock->dataBitmapOffset = 2;
}

void initBitmap()
{
    inodeBitmap = (InodeBitmap *)((char *)superblock + superblock->inodeBitmapOffset * superblock->blockSize);
    dataBitmap = (DataBitmap *)((char *)superblock + superblock->dataBitmapOffset * superblock->blockSize);
    for (int i = 0; i < 32; i++)
    {
        inodeBitmap->bitmap[i] = 0;
    }
    for (int i = 0; i < 32; i++)
    {
        dataBitmap->bitmap[i] = 0;
    }
}

void initInodeAndDataRegion()
{
    firstInode = (Inode *)((char *)superblock + superblock->inodeOffset * BLOCK_SIZE);
    firstDataRegion = (DataRegion *)((char *)superblock + superblock->dataOffset * BLOCK_SIZE);
}

// 遍历 inode 位图，返回第一个为 0 的位置编号，并将该值设置为 1
int getFreeInodeNoFromBitmap()
{
    int index = 0;
    for (int i = 0; i < 32; ++i)
    {
        for (int j = 31; j >= 0; --j)
        {
            if ((inodeBitmap->bitmap[i] & (1 << j)) == 0)
            {
                inodeBitmap->bitmap[i] |= (1 << j); // 将这个位置置为1
                goto done;
            }
            index++;
        }
    }
done:
    return index;
}

// 遍历 data 位图，返回第一个为 0 的位置编号，并将该值设置为 1
int getFreeDataNoFromBitmap()
{
    int index = 0;               // 初始化索引位置
    for (int i = 0; i < 32; ++i) // 遍历数据位图的每个int元素
    {
        for (int j = 31; j >= 0; --j) // 对于每个int，从高位到低位遍历每个二进制位
        {
            // 检查当前位是否为0，表示该inode未被分配
            if ((dataBitmap->bitmap[i] & (1 << j)) == 0)
            {
                // 找到未分配的位置，将其标记为已分配（置1），并结束循环
                dataBitmap->bitmap[i] |= (1 << j);
                goto done;
            }
            // 每检查一个位，索引递增
            index++;
        }
    }
done:
    // 返回分配的索引位置
    return index;
}

Inode *newInode(char *name, int fileType, Inode *ptrInode)
{
    // 分配内存来存储 Inode 结构体指针
    int no = getFreeInodeNoFromBitmap();
    Inode *inode = (Inode *)(firstInode + no);
    // 给指针指向的结构体的字段赋值
    inode->no = no;
    snprintf(inode->name, MAX_NAME_LENGTH, name); // 文件名
    inode->blockOffset = 0;
    inode->type = fileType;             // 文件类型为文件
    inode->size = 1024;                 // 文件大小为1024字节
    inode->createdAt = time(NULL);      // 创建时间为当前时间
    inode->lastAccessedAt = time(NULL); // 最后访问时间为当前时间
    inode->lastModifiedAt = time(NULL); // 最后修改时间为当前时间

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        // 当前目录指向自己，父目录指向上级目录
        inode->parentPtr = ptrInode->no;
    }
    else if (fileType == TYPE_DIRECTORY && strcmp(ROOT_PATH, name) != 0)
    {
        // 如果是文件夹，还需要初始化“.”和“..”
        Inode *onePointDir = newInode(".", TYPE_DIRECTORY, inode);
        Inode *twoPointDir = newInode("..", TYPE_DIRECTORY, curInode);
        inode->directPtr[0] = onePointDir->no;
        inode->directPtr[1] = twoPointDir->no;
        // 将剩下直接指针赋值为-1
    }
    for (int i = 0; i < DIRECT_POINTERS_COUNT; i++)
    {
        inode->directPtr[i] = -1;
    }
    inode->indirectPtr = -1;
    superblock->freeInodes--;
    return inode;
}

// 根据路径（可以是绝对路径或者当前目录的文件）找 Inode
// 注意“.”和“..”，需要进一步处理，对应的 parent 才是我们想要的节点
Inode *findInode(char *filePath)
{
    // step1.参数校验
    if (filePath == NULL)
    {
        return NULL;
    }
    if (strcmp(filePath, "/root") == 0)
    {
        return rootInode;
    }
    const char *prefix = "/";
    size_t prefix_len = strlen(prefix);

    // step2.比较是否为绝对路径
    if (strncmp(filePath, prefix, prefix_len) == 0)
    {

        // step2.1 为绝对路径，递归寻找目标目录。即遍历每个目录，一旦是目录就进入，再次执行以上操作，直到没有目录为止
        // 将路径拆分成一个个目录，看作此时的相对路径
        char path_copy[256];
        strncpy(path_copy, filePath, sizeof(path_copy));
        path_copy[sizeof(path_copy) - 1] = '\0';

        Inode *tmpInode = curInode;
        curInode = rootInode;
        char *token = strtok(path_copy, "/");
        if (token != NULL)
        {
            // 跳过根目录，因为此时已经在根目录当中了
            token = strtok(NULL, "/");
        }
        Inode *targetInode;
        while (token != NULL)
        {
            targetInode = findInode(token);
            if (targetInode == NULL)
            {
                curInode = tmpInode;
                return NULL;
            }
            if (targetInode->type == TYPE_FILE)
            {
                break;
            }
            curInode = targetInode;
            token = strtok(NULL, "/");
        }
        curInode = tmpInode;
        if (targetInode->type == TYPE_FILE && strcmp(token, targetInode->name) != 0)
        {
            printf("文件不能作为目录\n");
            return NULL;
        }

        return targetInode;
    }
    else
    {
        // step2.2 当前目录的文件，遍历寻找
        for (int i = 0; i < DIRECT_POINTERS_COUNT; i++)
        {
            int p = curInode->directPtr[i];
            if (p != -1 && strcmp(firstInode[p].name, filePath) == 0)
            {
                return (Inode *)(firstInode + p);
            }
        }
    }
    return NULL;
}

void freeInode(Inode *inode)
{
    inode->no = 0;
    inode->name[0] = '\0';
    inode->type = 0;
    inode->size = 0;
    inode->blockOffset = 0;
    inode->createdAt = 0;
    inode->lastAccessedAt = 0;
    inode->lastModifiedAt = 0;
    memset(inode->directPtr, 0, sizeof(inode->directPtr));
    inode->indirectPtr = 0;
    inode->parentPtr = 0;
    superblock->freeInodes++;
}

int isDirectoryEmpty(Inode *dir)
{
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (dir->directPtr[i] != -1)
        {
            return 0; // 非空
        }
    }
    return 1; // 空
}

// 获取数据区域中的当前数据长度
size_t getDataLength(DataRegion *dataRegion)
{
    // 遍历数据区域找到数据的末尾
    size_t length = 0;
    while (length < sizeof(dataRegion->data) && dataRegion->data[length] != '\0')
    {
        length++;
    }
    return length;
}

// 释放数据块
void freeDataBlock(int index)
{
    // 计算index在bitmap中的位置
    int i = index / 32; // 对应的int元素位置
    int j = index % 32; // 在int元素中的位位置

    // 将该位重置为0
    dataBitmap->bitmap[i] &= ~(1 << (31 - j));
}

void removeInode(Inode *inode)
{
    // 假设 inode 是通过编号从 firstInode 计算的偏移地址
    int inodeIndex = inode - firstInode;
    if (inodeIndex >= 0)
    {
        memset(inode, 0, sizeof(Inode)); // 清空 inode 结构
    }
}