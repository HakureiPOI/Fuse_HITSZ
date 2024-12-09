/* main.c源码 */
#define _XOPEN_SOURCE 700

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "fuse.h"
#include "../include/ddriver.h"
#include <linux/fs.h>
#include "pwd.h"
#include "unistd.h"
#include "string.h"

#define DEMO_DEFAULT_PERM        0777


/* 超级块 */
struct demo_super
{
    int     driver_fd;  /* 模拟磁盘的fd */

    int     sz_io;      /* 磁盘IO大小，单位B */
    int     sz_disk;    /* 磁盘容量大小，单位B */
    int     sz_blks;    /* 逻辑块大小，单位B */
};

/* 目录项 */
struct demo_dentry
{
    char    fname[128];
}; 

struct demo_super super;

#define DEVICE_NAME "ddriver"

/* 挂载文件系统 */
static void* demo_mount(struct fuse_conn_info * conn_info){
    // 打开驱动
    char device_path[128] = {0};
    sprintf(device_path, "%s/" DEVICE_NAME, getpwuid(getuid())->pw_dir);
    super.driver_fd = ddriver_open(device_path);

    printf("super.driver_fd: %d\n", super.driver_fd);


    /* 填充super信息 */
    /*
        int ddriver_ioctl(int fd, unsigned long cmd, void *ret);
        cmd: IOC_REQ_DEVICE_SIZE 请求设备大小
             IOC_REQ_DEVICE_IO_SZ 请求设备 I/O 大小
        ret: 用于接收来自设备驱动程序的数据
    */
    if (ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io) == -1) {
        perror("Failed to get device I/O size");
        ddriver_close(super.driver_fd);
        return (void*)-1;
    }
    printf("Device I/O size: %d bytes\n", super.sz_io);

    // 获取磁盘总大小
    if (ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_SIZE, &super.sz_disk) == -1) {
        perror("Failed to get device size");
        ddriver_close(super.driver_fd);
        return (void*)-1;
    }
    printf("Device size: %d bytes\n", super.sz_disk);

    // 设置逻辑块大小为I/O大小的两倍
    super.sz_blks = 2 * super.sz_io;
    printf("Block size set to: %d bytes\n", super.sz_blks);

    return 0;
}

/* 卸载文件系统 */
static void demo_umount(void* p){
    // 关闭驱动
    ddriver_close(super.driver_fd);
}

/* 遍历目录 */
static int demo_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    // 此处任务一同学无需关注demo_readdir的传入参数，也不要使用到上述参数

    char filename[128]; // 待填充的

    /* 根据超级块的信息，从第500逻辑块读取一个dentry，ls将只固定显示这个文件名 */

    /* TODO: 计算磁盘偏移off，并根据磁盘偏移off调用ddriver_seek移动磁盘头到磁盘偏移off处 */
    off_t off = super.sz_blks * 500;                 // 第500个逻辑块的偏移
    /*
        int ddriver_seek(int fd, off_t offset, int whence);
        fd: 文件描述符
        offset: 偏移量
        whence: 参考点，在 fs.h 中定义，SEEK_SET(0)是相对于文件开头
    */
    ddriver_seek(super.driver_fd, off, SEEK_SET);    // 将磁盘头移动

    /* TODO: 调用ddriver_read读出一个磁盘块到内存，512B */
    char readBuf[super.sz_io];                              // 缓冲区大小一个逻辑块
    /*
        int ddriver_read(int fd, char *buf, size_t size);
    */
    ddriver_read(super.driver_fd, readBuf, super.sz_io);    // 读取存入缓冲区

    /* TODO: 使用memcpy拷贝上述512B的前sizeof(demo_dentry)字节构建一个demo_dentry结构 */
    struct demo_dentry dentry;
    memcpy(&dentry, readBuf, sizeof(dentry));

    /* TODO: 填充filename */
    strcpy(filename, dentry.fname);

    // 此处大家先不关注filler，已经帮同学写好，同学填充好filename即可
    return filler(buf, filename, NULL, 0);
}

/* 显示文件属性 */
static int demo_getattr(const char* path, struct stat *stbuf)
{
    if(strcmp(path, "/") == 0)
        stbuf->st_mode = DEMO_DEFAULT_PERM | S_IFDIR;            // 根目录是目录文件
    else
        stbuf->st_mode = 0644 | S_IFREG;            // 该文件显示为普通文件
    return 0;
}

/* 根据任务1需求 只用实现前四个钩子函数即可完成ls操作 */
static struct fuse_operations ops = {
	.init = demo_mount,						          /* mount文件系统 */		
	.destroy = demo_umount,							  /* umount文件系统 */
	.getattr = demo_getattr,							  /* 获取文件属性 */
	.readdir = demo_readdir,							  /* 填充dentrys */
};

int main(int argc, char *argv[])
{
    int ret = 0;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    ret = fuse_main(args.argc, args.argv, &ops, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
