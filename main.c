#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "filesystem.h"
#include "operate.h"

void writeTerminalHead()
{
    printf("%s> ", curPath);
}

void initPath()
{
    curPath = calloc(1, sizeof(char) * 100);
    strcpy(curPath, "/");
    strcat(curPath, ROOT_PATH);
}

void initFile()
{
    // step1.初始化信息
    file_disk = calloc(BLOCK_TOTAL_NUM, BLOCK_SIZE);
    initSuperblock();
    initBitmap();
    initInodeAndDataRegion();

    // step2.初始化根目录
    // 创建 3 个 inode：根目录，当前目录“.”，上级目录“..”。这里的上级目录指向自己
    rootInode = newInode(ROOT_PATH, TYPE_DIRECTORY, NULL);
    Inode *onePointDir = newInode(".", TYPE_DIRECTORY, rootInode);
    Inode *twoPointDir = newInode("..", TYPE_DIRECTORY, rootInode);
    // 根目录直接指针指向“.”和“..” inode
    rootInode->directPtr[0] = onePointDir->no;
    rootInode->directPtr[1] = twoPointDir->no;

    // step3.初始化文件路径
    initPath();

    // step4.将数据持久化到文件
    FILE *fp = fopen(DISK_NAME, "wb");
    fwrite(file_disk, 1, BLOCK_TOTAL_SIZE, fp);
    fclose(fp);
}

// 将磁盘文件映射到内存中
void diskToMemory()
{
    // step1.读去文件内容至内存
    FILE *fp = fopen(DISK_NAME, "rb");
    if (fp == NULL)
    {
        perror("Failed to open file");
        return;
    }
    file_disk = calloc(BLOCK_TOTAL_NUM, BLOCK_SIZE);
    fread(file_disk, 1, BLOCK_TOTAL_SIZE, fp);
    fclose(fp);

    // step2.初始化
    superblock = (Superblock *)file_disk;
    inodeBitmap = (InodeBitmap *)((char *)superblock + superblock->inodeBitmapOffset * superblock->blockSize);
    dataBitmap = (DataBitmap *)((char *)superblock + superblock->dataBitmapOffset * superblock->blockSize);
    firstInode = (Inode *)((char *)superblock + superblock->inodeOffset * superblock->blockSize);
    firstDataRegion = (DataRegion *)((char *)superblock + superblock->dataOffset * superblock->blockSize);
    initPath();

    // step3.设置根目录和当前目录
    rootInode = (Inode *)(firstInode);
    curInode = rootInode;
}

void doMain()
{
    writeTerminalHead();
    int running = 1;
    while (running)
    {
        char command[100];
        char option1[50];
        char option2[50];
        scanf("%s", command);

        // 将指令映射为对应的索引
        int commandIdx = -1;
        for (int i = 0; i < commandNum; i++)
        {
            if (strcmp(commandArr[i], command) == 0)
            {
                commandIdx = i;
                break;
            }
        }

        switch (commandIdx)
        {
        case OP_HELP:
            help();
            writeTerminalHead();
            break;
        case OP_LS:
            ls();
            writeTerminalHead();
            break;
        case OP_MKDIR:
            scanf("%s", option1);
            mkdir(option1);
            writeTerminalHead();
            break;
        case OP_RMDIR:
            scanf("%s", option1);
            rmdir(option1);
            writeTerminalHead();
            break;
        case OP_RN:
            scanf("%s", option1);
            scanf("%s", option2);
            rn(option1, option2);
            writeTerminalHead();
            break;
        case OP_CD:
            scanf("%s", option1);
            cd(option1);
            writeTerminalHead();
            break;
        case OP_TOUCH:
            scanf("%s", option1);
            scanf("%s", option2);
            touch(option1, option2);
            writeTerminalHead();
            break;
        case OP_WRITE:
            scanf("%s", option1);
            write(option1);
            writeTerminalHead();
            break;
        case OP_CAT:
            scanf("%s", option1);
            cat(option1);
            writeTerminalHead();
            break;
        case OP_MV:
            scanf("%s", option1);
            scanf("%s", option2);
            mv(option1, option2);
            writeTerminalHead();
            break;
        case OP_DEL:
            scanf("%s", option1);
            del(option1);
            writeTerminalHead();
            break;
        case OP_CP:
            scanf("%s", option1);
            scanf("%s", option2);
            cp(option1, option2);
            writeTerminalHead();
            break;
        case OP_FIND:
            scanf("%s", option1);
            find(option1);
            writeTerminalHead();
            break;
        case OP_SYNC:
            sync();
            writeTerminalHead();
            break;
        case OP_EXIT:
            exit_program();
            running = 0;
            break;
        case OP_DF:
            df();
            writeTerminalHead();
            break;
        default:
            printf("ERROR: 命令有误\n");
            writeTerminalHead();
            break;
        }
    }
}

void start()
{
    FILE *fp;
    if ((fp = fopen(DISK_NAME, "rb")) == NULL)
    {
        initFile();
    }
    diskToMemory();
    help();
    doMain();
}

int main()
{
    start();
    return 0;
}