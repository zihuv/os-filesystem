#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#define OP_HELP 0
#define OP_LS 1
#define OP_MKDIR 2
#define OP_RMDIR 3
#define OP_RN 4
#define OP_CD 5
#define OP_TOUCH 6
#define OP_WRITE 7
#define OP_CAT 8
#define OP_MV 9
#define OP_DEL 10
#define OP_CP 11
#define OP_FIND 12
#define OP_SYNC 13
#define OP_EXIT 14
#define OP_DF 15

char *commandArr[16] = {
    "help",
    "ls",
    "mkdir",
    "rmdir",
    "rn",
    "cd",
    "touch",
    "write",
    "cat",
    "mv",
    "del",
    "cp",
    "find",
    "sync",
    "exit",
    "df"};

int commandNum = sizeof(commandArr) / sizeof(commandArr[0]);

void removeInode(Inode *inode);
void del(char *name);
void rmdir(char *name);

// 清空标准输入缓冲区
void clearInputBuffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

// 将路径拆分成目录名和文件名，结果分别放入 dirpath 和 filename。比如：/root/abc = /root + abc
void getDirAndFilenameByPath(char *path, char *dirpath, char *filename)
{
    char *lastSlash = strrchr(path, '/');
    if (lastSlash == NULL)
    {
        // 如果路径中没有 '/', 则整个路径视为文件名
        strcpy(filename, path);
        strcpy(dirpath, curPath);
    }
    else
    {
        strncpy(dirpath, path, lastSlash - path);
        dirpath[lastSlash - path] = '\0'; // 添加终止符
        // 复制'/'之后的内容到filename
        strcpy(filename, lastSlash + 1);
    }
}

void formatTime(char *buffer, time_t time)
{
    struct tm *tm_info = localtime(&time);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 递归删除目录
void rmdirRecursive(Inode *dir)
{
    // 遍历目录 dir 中的文件
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (dir->directPtr[i] != -1)
        {
            Inode *entry = firstInode + dir->directPtr[i];
            if (entry->type == TYPE_DIRECTORY)
            {
                rmdir(entry->name); // 删除子目录。如果子目录中依然有文件，会继续递归删除
            }
            else if (entry->type == TYPE_FILE)
            {
                del(entry->name); // 删除文件
            }
        }
    }
}

// -----命令实现-----

void help()
{
    printf("=====================================================================\n\n");
    printf("* help\t\t无\t\t\t提示信息\n");

    printf("* ls\t\t无\t\t\t查看<当前目录>的所有文件\n");
    printf("* mkdir\t\t目录名\t\t\t在<当前目录>下创建文件夹\n");
    printf("* rmdir\t\t目录名\t\t\t在<当前目录>下删除指定目录\n");
    printf("* rn\t\t旧名 新名\t\t在<当前目录>下重命名文件\n");
    printf("* cd\t\t目录名\t\t\t切换到该目录<绝对路径或当前目录>\n");

    printf("* touch\t\t文件名 大小\t\t在<当前目录>下创建指定大小文件\n");
    printf("* write\t\t文件名\t\t\t从文件内容末尾写数据(英文字母)到该文件\n");
    printf("* cat\t\t文件名\t\t\t读取该文件内容\n");
    printf("* del\t\t文件名\t\t\t删除指定文件<绝对路径或当前目录>\n");
    printf("* cp\t\t文件名 新文件名\t\t复制文件<绝对路径或当前目录>\n");
    printf("* mv\t\t源文件 目标文件\t\t将源文件移动至目标文件<绝对路径或当前目录>\n");
    printf("* find\t\t文件名\t\t\t在<当前目录>中查找文件名路径\n");

    printf("* df\t\t无\t\t\t文件系统的磁盘空间使用情况\n");
    printf("* sync\t\t无\t\t\t文件持久化\n");
    printf("* exit\t\t无\t\t\t退出系统\n");
    printf("\n=====================================================================\n\n");
}

void ls()
{
    printf("文件名称\t文件类型\t创建时间\t\t最后访问时间\t\t最后修改时间\n");
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        char createdAt[20];
        char lastAccessedAt[20];
        char lastModifiedAt[20];

        int p = curInode->directPtr[i];
        if (p == -1)
        {
            continue;
        }
        Inode *curFileInode = (Inode *)(firstInode + p);
        char *type = curFileInode->type == TYPE_DIRECTORY ? "目录" : "文件";
        formatTime(createdAt, curFileInode->createdAt);
        formatTime(lastAccessedAt, curFileInode->lastAccessedAt);
        formatTime(lastModifiedAt, curFileInode->lastModifiedAt);
        printf("%s\t\t<%s>\t\t%s\t%s\t%s\n", curFileInode->name, type, createdAt, lastAccessedAt, lastModifiedAt);
    }
}

void mkdir(char *name)
{
    if (name[0] == '.')
    {
        printf("文件命名格式错误\n");
        return;
    }
    if (strlen(name) > MAX_NAME_LENGTH)
    {
        printf("名称不能超过 %d 个字符\n", MAX_NAME_LENGTH - 1);
        return;
    }
    // 检查名称中是否包含非法字符 "/"
    for (int i = 0; name[i] != '\0'; i++)
    {
        if (name[i] == '/')
        {
            printf("名称中不能包含字符 '/'。\n");
            return;
        }
    }
    
    Inode *newDir = newInode(name, TYPE_DIRECTORY, NULL);

    for (int i = 0; i < DIRECT_POINTERS_COUNT; i++)
    {
        int p = curInode->directPtr[i];
        Inode *tmp = (Inode *)(firstInode + p);
        if (strcmp(name, tmp->name) == 0)
        {
            printf("文件名不能重名\n");
            return;
        }
        if (p == -1)
        {
            curInode->directPtr[i] = newDir->no;
            Inode *onePointDir = newInode(".", TYPE_DIRECTORY, newDir);
            Inode *twoPointDir = newInode("..", TYPE_DIRECTORY, curInode);
            newDir->directPtr[0] = onePointDir->no;
            newDir->directPtr[1] = twoPointDir->no;
            break;
        }
        else if (i == DIRECT_POINTERS_COUNT - 1 && p != -1)
        {
            printf("当前目录文件到达上限");
            return;
        }
    }
}

void rmdir(char *name)
{
    // step1.参数校验
    if (name[0] == '/')
    {
        printf("不允许使用绝对路径删除文件夹\n");
        return;
    }
    Inode *dir = findInode(name);
    if (dir == NULL)
    {
        printf("目录：%s 不存在\n", name);
        return;
    }

    // step2.判断目录是否为空。如果为空，就递归删除文件
    if (!isDirectoryEmpty(dir))
    {
        char choice;
        printf("目录：%s 包含其它文件，是否递归删除？(y/n): ", dir->name);
        scanf(" %c", &choice);
        if (choice != 'y' && choice != 'Y')
        {
            return;
        }
        Inode *tmpInode = curInode;
        curInode = dir;
        rmdirRecursive(dir); // 递归删除
        curInode = tmpInode;
    }

    // 从当前目录中删除该目录的 inode 信息
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        // 跳过 . 和 ..
        if (curInode->directPtr[i] == dir->no)
        {
            curInode->directPtr[i] = -1;
            break;
        }
    }

    // 释放 inode
    freeInode(dir);
}

void rn(char *oldName, char *newName)
{
    // 查找当前目录中名为 oldName 的 inode
    Inode *targetInode = findInode(oldName);
    if (targetInode == NULL)
    {
        printf("文件：%s 不存在，可以使用 touch 创建文件\n", oldName);
        return;
    }

    // 检查 newName 是否在当前目录中已经存在
    if (findInode(newName) != NULL)
    {
        printf("目标文件或目录已经存在\n");
        return;
    }

    // 更新 inode 的 name 字段为 newName
    strncpy(targetInode->name, newName, MAX_NAME_LENGTH - 1);
    targetInode->name[MAX_NAME_LENGTH - 1] = '\0'; // 确保字符串以 null 结尾

    printf("重命名 %s 为 %s\n", oldName, newName);

    targetInode->lastAccessedAt = time(NULL);
    targetInode->lastModifiedAt = time(NULL);
}

void cd(char *name)
{
    if (name == NULL || strlen(name) == 0)
    {
        printf("无效目录\n");
        return;
    }
    if (name[0] == '/' && strncmp("/root", name, strlen("/root")) != 0)
    {
        printf("路径：%s 错误\n", name);
        return;
    }
    // step1.查询节点
    Inode *targetInode = findInode(name);
    if (targetInode == NULL || targetInode->type != TYPE_DIRECTORY)
    {
        printf("目录不存在\n");
        return;
    }
    if (strcmp(targetInode->name, ".") == 0 || strcmp(targetInode->name, "..") == 0)
    {
        targetInode = (Inode *)(firstInode + targetInode->parentPtr);
    }

    // step2.判断是相对路径还是绝对路径
    char *path = calloc(1, 64);
    if (name[0] == '/')
    {
        strcpy(path, name);
    }
    else
    {
        // curPath + name
        strcat(path, curPath);
        if (strcmp(name, ".") == 0 || (strcmp(name, "..") == 0 && strcmp(curPath, "/root") == 0))
        {
            // 不做任何操作
        }
        else if (strcmp(name, "..") == 0)
        {
            // 回退上级目录
            char *lastSlash = strrchr(path, '/');
            *lastSlash = '\0';
        }
        else
        {
            strcat(path, "/");
            strcat(path, name);
        }
    }
    // step3.修改全局变量
    curInode = targetInode;
    curPath = path;
    // step4.修改访问时间
    curInode->lastAccessedAt = time(NULL);
}

void touch(char *name, char *size)
{
    // step1.参数校验
    if (size == NULL || *size == '\0')
    {
        printf("文件大小不能为空或者为字母\n");
        return;
    }
    if (strlen(name) > MAX_NAME_LENGTH)
    {
        printf("名称不能超过 %d 个字符\n", MAX_NAME_LENGTH - 1);
        return;
    }
    for (const char *p = size; *p; p++)
    {
        if (!isdigit(*p))
        {
            printf("文件大小只能输入整数数字\n");
            return;
        }
    }
    if (name[0] == '.' || name[0] == '/')
    {
        printf("文件名称不合法\n");
        return;
    }
    // 检查名称中是否包含非法字符 "/"
    for (int i = 0; name[i] != '\0'; i++)
    {
        if (name[i] == '/')
        {
            printf("名称中不能包含字符 '/'。\n");
            return;
        }
    }

    if (findInode(name) != NULL)
    {
        printf("文件：%s 已存在，不允许重名\n", name);
        return;
    }

    // step2.计算文件大小并创建新 Inode
    int fileSize = atoi(size) * 1024;                          // 转换为字节
    int blockCount = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE; // 需要的数据块数量
    if (blockCount > BLOCK_TOTAL_NUM / 2)
    {
        printf("文件：%s 申请空间过大，创建失败\n", name);
        return;
    }

    Inode *newFile = newInode(name, TYPE_FILE, curInode);
    newFile->size = fileSize;

    // step3.分配数据块
    int directPtrIndex = 0;
    char *idxFileDataRegion;
    int offset = 0;
    for (int i = 0; i < blockCount; i++)
    {
        // 先使用直接指针
        if (directPtrIndex < DIRECT_POINTERS_COUNT)
        {
            newFile->directPtr[directPtrIndex++] = getFreeDataNoFromBitmap();
        }
        else
        {
            // 超过直接指针数，需要使用间接指针
            if (newFile->indirectPtr == -1)
            {
                int idxFileDataRegionNo = getFreeDataNoFromBitmap();
                idxFileDataRegion = (char *)(firstDataRegion + idxFileDataRegionNo);
                newFile->indirectPtr = idxFileDataRegionNo;
            }
            int no = getFreeDataNoFromBitmap();
            memcpy(idxFileDataRegion + offset, &no, sizeof(int));
            offset += sizeof(int);
        }
    }

    // step4.在当前目录添加新的索引节点（索引节点是孤立的，需要与当前目录建立联系，当前目录节点才能找到这个索引节点）
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (curInode->directPtr[i] == -1)
        {
            curInode->directPtr[i] = newFile->no;
            break;
        }
    }

    printf("文件 %s 已创建，大小为 %d KB\n", name, fileSize / 1024);
}

// 往文件末尾追加数据
void write(char *name)
{
    // step1.参数校验
    Inode *file = findInode(name);
    if (file == NULL)
    {
        printf("文件：%s 不存在\n", name);
        return;
    }

    if (file->type != TYPE_FILE)
    {
        printf("%s 不是一个文件\n", name);
        return;
    }

    // step2.读取用户输入
    printf("请输入要追加的内容：\n");
    clearInputBuffer();
    char input[1024];
    fgets(input, sizeof(input), stdin);
    // 将换行符替换成'\0'
    input[strcspn(input, "\n")] = '\0';
    DataRegion *dataRegion = (DataRegion *)(file->directPtr[0] + firstDataRegion);
    if (dataRegion == NULL)
    {
        printf("为 Null\n");
        return;
    }
    // 获取当前数据的长度
    size_t currentLength = getDataLength(dataRegion);

    // step3.检查是否有足够的空间追加新数据
    size_t inputLength = strlen(input);
    if (currentLength + inputLength >= sizeof(dataRegion->data))
    {
        printf("数据区域空间不足，无法追加新数据\n");
        return;
    }

    // step4.追加新数据到现有数据之后
    memcpy(dataRegion->data + currentLength, input, inputLength);
    dataRegion->data[currentLength + inputLength] = '\0';

    file->lastAccessedAt = time(NULL);
    file->lastModifiedAt = time(NULL);
}

// 显示文件内容
void cat(char *name)
{
    // 查找当前目录中名为 name 的文件 inode
    Inode *file = findInode(name);
    if (file == NULL)
    {
        printf("文件不存在\n");
        return;
    }

    if (file->type != TYPE_FILE)
    {
        printf("%s 不是一个文件\n", name);
        return;
    }

    printf("文件内容：");
    for (int i = 0; i < DIRECT_POINTERS_COUNT; i++)
    {
        int blockNo = file->directPtr[i];
        if (blockNo != -1)
        {
            printf("%s", (char *)(firstDataRegion + blockNo));
        }
    }
    printf("\n");
    file->lastAccessedAt = time(NULL);
}

void del(char *name)
{
    char dirpath[50]; // 分割后的目录路径
    char filename[50];
    getDirAndFilenameByPath(name, dirpath, filename);

    Inode *file = findInode(name);
    if (file == NULL)
    {
        printf("文件不存在\n");
        return;
    }
    if (file->type != TYPE_FILE)
    {
        printf("%s 不是一个文件\n", name);
        return;
    }

    // 释放数据块
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (file->directPtr[i] != -1)
        {
            freeDataBlock(file->directPtr[i]);
            file->directPtr[i] = -1;
        }
    }

    //  如果有间接指针，需要处理间接数据块
    if (file->indirectPtr != -1)
    {
        // 这里假设简单的间接指针处理逻辑
        int offset = 0;
        int *indirectBlock = (int *)(file->indirectPtr + firstDataRegion);
        while (1)
        {
            int *secondBlockPtr = indirectBlock + offset;
            offset++;
            if (*secondBlockPtr == 0)
            {
                break;
            }
            freeDataBlock(*secondBlockPtr);
        }
        freeDataBlock(file->indirectPtr);
        file->indirectPtr = -1;
    }

    // 从目录中删除目录项
    Inode *tmpInode = findInode(dirpath);
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (tmpInode->directPtr[i] != -1)
        {
            Inode *inode = firstInode + tmpInode->directPtr[i];
            if (inode != NULL && strcmp(inode->name, filename) == 0)
            {
                tmpInode->directPtr[i] = -1; // 删除目录项
                break;
            }
        }
    }

    // 删除 inode
    removeInode(file);
}

void cp(char *oldName, char *newName)
{
    if (oldName == NULL || strlen(oldName) == 0 || newName == NULL || strlen(newName) == 0)
    {
        printf("无效的源或目标名称\n");
        return;
    }
    if (findInode(newName) != NULL)
    {
        printf("文件名：%s 已经在该目录存在，不允许以该文件名复制\n", newName);
        return;
    }

    // step1.创建新的 Inode
    Inode *oldFile = findInode(oldName);
    if (oldFile == NULL)
    {
        printf("被复制的文件：%s 不存在\n", oldName);
        return;
    }
    if (oldFile->type == TYPE_DIRECTORY)
    {
        printf("不允许复制文件夹\n");
        return;
    }

    char dirpath[strlen(newName) + 1]; // 分割后的目录路径
    char filename[strlen(newName) + 1];
    getDirAndFilenameByPath(newName, dirpath, filename);
    Inode *newFile = newInode(filename, oldFile->type, NULL);

    // step2.复制一份新的数据区，新 Inode 指向这个数据区
    int dataNo = getFreeDataNoFromBitmap();
    memcpy(firstDataRegion + dataNo, firstDataRegion + oldFile->directPtr[0], BLOCK_SIZE); // 考虑只有一个数据区的情况
    newFile->directPtr[0] = dataNo;

    // step3.复制其它信息
    strcpy(newFile->name, filename);
    newFile->size = oldFile->size;
    newFile->blockOffset = oldFile->blockOffset;
    newFile->createdAt = time(NULL);
    newFile->lastAccessedAt = time(NULL);
    newFile->lastModifiedAt = time(NULL);
    newFile->indirectPtr = oldFile->indirectPtr;
    newFile->parentPtr = oldFile->parentPtr;

    // step4.目录直接指针指向复制的 Inode
    Inode *targetInode = findInode(dirpath);
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (targetInode->directPtr[i] == -1)
        {
            targetInode->directPtr[i] = newFile->no;
            break;
        }
    }

    // step5.修改时间
    oldFile->lastModifiedAt = time(NULL);
    oldFile->lastAccessedAt = time(NULL);
}

void mv(char *source, char *destination)
{
    // step1.参数校验
    if (source == NULL || strlen(source) == 0 || destination == NULL || strlen(destination) == 0 || source[0] == '.' || destination[0] == '.')
    {
        printf("无效的源或目标路径\n");
        return;
    }
    if (findInode(source) == NULL)
    {
        printf("文件名：%s 在该目录不存在\n", source);
        return;
    }
    if (findInode(destination) != NULL)
    {
        printf("文件名：%s 在该目录已经存在，不允许以该文件名复制\n", destination);
        return;
    }

    // step2.先复制文件，再将源文件删除
    cp(source, destination);
    del(source);
}

void find(char *fileName)
{
    // 递归寻找所有目录
    for (int i = 2; i < DIRECT_POINTERS_COUNT; i++)
    {
        if (curInode->directPtr[i] != -1)
        {
            Inode *nxtInode = (Inode *)(firstInode + curInode->directPtr[i]);
            // 如果是目录，就递归寻找
            if (nxtInode->type == TYPE_DIRECTORY)
            {
                Inode *tmpInode = curInode;
                char *tmpPath = strdup(curPath);
                curInode = nxtInode;
                strcat(curPath, "/");
                strcat(curPath, nxtInode->name);
                find(fileName);
                curInode = tmpInode;
                strcpy(curPath, tmpPath);
            }
            if (strcmp(nxtInode->name, fileName) == 0)
            {
                char *type = nxtInode->type == TYPE_FILE ? "文件" : "目录";
                printf("<%s> %s/%s\n", type, curPath, nxtInode->name);
            }
        }
    }
}

void sync()
{
    FILE *fp = fopen(DISK_NAME, "wb");
    fwrite(file_disk, 1, BLOCK_TOTAL_SIZE, fp);
    fclose(fp);
}

void exit_program()
{
    printf("退出系统\n");
    sync();
}

void df()
{
    printf("文件系统摘要:\n");
    printf("总数据块数: %d\n", superblock->totalBlocks);
    printf("数据块大小: %d 字节\n", superblock->blockSize);
    printf("索引节点总数: %d\n", superblock->inodeCount);
    printf("索引节点空闲数: %d\n", superblock->freeInodes);
    printf("索引节点大小: %d 字节\n", superblock->inodeSize);
    printf("索引节点起点位置: %d (地址: 0x%llx)\n", superblock->inodeOffset, (uintptr_t)firstInode);
    printf("数据块起点位置: %d (地址: 0x%llx)\n", superblock->dataOffset, (uintptr_t)firstDataRegion);
    printf("索引节点位图起点位置: %d (地址: 0x%llx)\n", superblock->inodeBitmapOffset, (uintptr_t)inodeBitmap);
    printf("数据块位图起点位置: %d (地址: 0x%llx)\n", superblock->dataBitmapOffset, (uintptr_t)dataBitmap);
}