
#include "params.h"

#include <cstdio>
#include <cstdlib>

#include <string>
#include <iostream>
#include <algorithm>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "inet.h"
#include "log.h"
#include "test.pb.h"

using namespace std;

//static int sockfd;

pthread_mutex_t mutex_sock;
static fuse_operations bb_oper;

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void bb_fullpath(char fpath[PATH_MAX], const char *path)
{
  strcpy(fpath, BB_DATA->dir_ssd);
  strncat(fpath, path, PATH_MAX); // ridiculously long paths will
  ////log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n", BB_DATA->rootdir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int bb_getattr(const char *path, struct stat *statbuf)
{
  int retstat;
  char fpath[PATH_MAX];   // fpath is rootdir, and path is like ssd.
  int sockfd;
  //int i;

  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::getattr obj_send, obj_recv;

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Getattr : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Getattr : can’t connect to server \n");
    return -1;
  }
  /*i = 0;
  while (1) {
    printf("First time here - %d, %d, self : %d\n", sckbsk[i].sfd, sckbsk[i].available, pthread_self());
    if (sckbsk[i].available) {
      pthread_mutex_lock(&temp);
      sockfd = sckbsk[i].sfd;
      sckbsk[i].available = false;
      pthread_mutex_unlock(&temp);
      break;
    }
    i = (i+1)%NUM_THREADS;
  }*/

  //pthread_mutex_lock(&mutex_sock);
  /////// Send getattr request to server.
  if (::write(sockfd, "getattr", 7) < 7) {
    printf("Getattr : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-getattr") != 0) {
      printf("Getattr : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Getattr : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to get attributes
  obj_send.set_path(path);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Getattr : [Write path] error\n");
    return -1;
  }

  ////// Receive the attributes information formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Getattr : [Read stat] error\n");
    printf("path : %s\n", path);
    return -1;
  }

  ////// Parse the FormatTransfer::getattr object to stat structure
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  //if (obj_recv.ret() == 0) {

  statbuf->st_dev = obj_recv.dev();
  statbuf->st_ino = obj_recv.inode();
  statbuf->st_mode = obj_recv.mode();
  statbuf->st_nlink = obj_recv.nlink();
  statbuf->st_uid = obj_recv.uid();
  statbuf->st_gid = obj_recv.gid();

  ////// Because of this, I got a problem!!!!!
  //statbuf->st_rdev = obj_recv.devid();
  
  statbuf->st_size = obj_recv.size();
  statbuf->st_blksize = obj_recv.blksize();
  statbuf->st_blocks = obj_recv.nblk();
  statbuf->st_atime = obj_recv.atime();
  statbuf->st_mtime = obj_recv.mtime();
  statbuf->st_ctime = obj_recv.ctime();
  retstat = obj_recv.ret();
  //}
  //else
  //  retstat = -1;
  
  //log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
    
  //printf("[getattr] %s's size is : %d\n", path, statbuf->st_size);
  //log_stat(statbuf);

  ::close(sockfd);
  //sckbsk[i].available = true;
  pthread_mutex_unlock(&mutex_sock);
  return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int bb_readlink(const char *path, char *link, size_t size)
{
  int retstat;
  char fpath[PATH_MAX];

  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::readlink obj_send, obj_recv;

  //log_msg("bb_readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link, size);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Readlink : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Readlink : can’t connect to server \n");
    return -1;
  }

  /////// Send readlink request to server.
  if (::write(sockfd, "readlink", 8) < 8) {
    printf("Readlink : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-readlink") != 0) {
      printf("Readlink : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Readlink : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path, link name, size
  obj_send.set_path(path);
  obj_send.set_linkpath(link);
  obj_send.set_size(size);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Readlink : [Write path and link and size] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Readlink : [Read ret] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  ////// Parse the FormatTransfer::getattr object to stat structure
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  retstat = obj_recv.ret();
  if (retstat >= 0) {
    link[retstat] = '\0';
    retstat = 0;
  }

  return retstat;
  /*
  //log_msg("bb_readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link, size);
  bb_fullpath(fpath, path);

  retstat = log_syscall("fpath", readlink(fpath, link, size - 1), 0);
  if (retstat >= 0) {
    link[retstat] = '\0';
    retstat = 0;
  }
    
  return retstat;
  */
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int bb_mknod(const char *path, mode_t mode, dev_t dev)
{
  int retstat;
  char fpath[PATH_MAX];
    
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::mknod obj_send, obj_recv;

  //printf("[mknod] here\n");
  //printf("mode : %d, %u\n", sizeof(mode), mode);
  //fprintf(stdout, "%s", mode);
  //log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Mknod : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Mknod : can’t connect to server \n");
    return -1;
  }

  /////// Send mknod request to server.
  if (::write(sockfd, "mknod1", 6) < 6) {
    printf("Mknod : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-mknod1") != 0) {
      printf("Mknod : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Mknod : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send data about mknod to server
  obj_send.set_path(path);
  obj_send.set_mode(mode);

  //cout << "path : " << path;
  //cout << ", mode : " << mode << endl;

  if (S_ISREG(mode)) {
    //cout << "[mknod] regular file" << endl;

    obj_send.set_command("open");
    obj_send.SerializeToString(&str_send);

    if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
      printf("Mknod : [Write path and mode] error\n");
      return -1;
    }
    if (::read(sockfd, recv_buf, BLOCK_SIZE) >= 0) {
      str_recv = recv_buf;
      obj_recv.ParseFromString(str_recv);
      retstat = obj_recv.ret();
    }

  }
  else {

    if (S_ISFIFO(mode)) {
      //cout << "[mknod] fifo file" << endl;
      obj_send.set_command("mkfifo");
      obj_send.SerializeToString(&str_send);

      if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
        printf("Mkfifo : [Write path and mode] error\n");
        return -1;
      }
      if (::read(sockfd, recv_buf, BLOCK_SIZE) >= 0) {
        str_recv = recv_buf;
        obj_recv.ParseFromString(str_recv);
        retstat = obj_recv.ret();
      }
    }
    else {
      //cout << "[mknod] dev file" << endl;
      obj_send.set_command("mknod");
      obj_send.set_dev(dev);
      obj_send.SerializeToString(&str_send);

      if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
        printf("Mknod : [Write path and mode and dev] error\n");
        return -1;
      }
      if (::read(sockfd, recv_buf, BLOCK_SIZE) >= 0) {
        str_recv = recv_buf;
        obj_recv.ParseFromString(str_recv);
        retstat = obj_recv.ret();
      }
    }

  }
  pthread_mutex_unlock(&mutex_sock);
  ::close(sockfd);
  return retstat;

  /*
  //log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);
  bb_fullpath(fpath, path);
    
  // On Linux this could just be 'mknod(path, mode, dev)' but this
  // tries to be be more portable by honoring the quote in the Linux
  // mknod man page stating the only portable use of mknod() is to
  // make a fifo, but saying it should never actually be used for
  // that.
  if (S_ISREG(mode)) {
    retstat = log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat >= 0)
      retstat = log_syscall("close", close(retstat), 0);
  }
  else {
    if (S_ISFIFO(mode))
      retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
    else
      retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);
  }
    
  return retstat;
  */
}

/** Create a directory */
int bb_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
    bb_fullpath(fpath, path);

    return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int bb_unlink(const char *path)
{
  char fpath[PATH_MAX];
  
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::unlink obj_send, obj_recv;

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Unlink : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Unlink : can’t connect to server \n");
    return -1;
  }

  /////// Send open request to server.
  if (::write(sockfd, "unlink", 6) < 6) {
    printf("Open : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-unlink") != 0) {
      printf("Unlink : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Unlink : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to unlink
  obj_send.set_path(path);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Unlink : [Write path] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Unlink : [Read ret] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    return 0;
  }
  else {
    return -1;
  }
  /*
  //log_msg("bb_unlink(path=\"%s\")\n", path);
  bb_fullpath(fpath, path);

  return log_syscall("unlink", unlink(fpath), 0);
  */
}

/** Remove a directory */
int bb_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    
    //log_msg("bb_rmdir(path=\"%s\")\n", path);
    bb_fullpath(fpath, path);

    return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int bb_symlink(const char *path, const char *link)
{
  char flink[PATH_MAX];
    
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::symlink obj_send, obj_recv;

  //log_msg("\nbb_symlink(path=\"%s\", link=\"%s\")\n", path, link);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Symlink : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Symlink : can’t connect to server \n");
    return -1;
  }

  /////// Send symlink request to server.
  if (::write(sockfd, "symlink", 7) < 7) {
    printf("Symlink : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-symlink") != 0) {
      printf("Symlink : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Symlink : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to symlink
  obj_send.set_path(path);
  obj_send.set_linkpath(link);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Symlink : [Write path] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Symlink : [Read ret] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    return 0;
  }
  else {
    return -1;
  }

  /*
  bb_fullpath(flink, link);

  return log_syscall("symlink", symlink(path, flink), 0);
  */
}

/** Rename a file */
// both path and newpath are fs-relative
int bb_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    //log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);
    bb_fullpath(fpath, path);
    bb_fullpath(fnewpath, newpath);

    return log_syscall("rename", rename(fpath, fnewpath), 0);
}

/** Create a hard link to a file */
int bb_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    //log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n", path, newpath);
    bb_fullpath(fpath, path);
    bb_fullpath(fnewpath, newpath);

    return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int bb_chmod(const char *path, mode_t mode)
{
  char fpath[PATH_MAX];
  
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::chmod obj_send, obj_recv;

  //log_msg("\nbb_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Chmod : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Chmod : can’t connect to server \n");
    return -1;
  }

  /////// Send chmod request to server.
  if (::write(sockfd, "chmod", 5) < 5) {
    printf("Chmod : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-chmod") != 0) {
      printf("Chmod : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Chmod : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to chmod
  obj_send.set_path(path);
  obj_send.set_mode(mode);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Chmod : [Write path and mode] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Chmod : [Read ret] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    return 0;
  }
  else {
    return -1;
  }

  /*
  //log_msg("\nbb_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
  bb_fullpath(fpath, path);

  return log_syscall("chmod", chmod(fpath, mode), 0);
  */
}

/** Change the owner and group of a file */
int bb_chown(const char *path, uid_t uid, gid_t gid)
  
{
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
    bb_fullpath(fpath, path);

    return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int bb_truncate(const char *path, off_t newsize)
{
  char fpath[PATH_MAX];

  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::truncate obj_send, obj_recv;

  //log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Truncate : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Truncate : can’t connect to server \n");
    return -1;
  }

  /////// Send truncate request to server.
  if (::write(sockfd, "truncate", 8) < 8) {
    printf("Truncate : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-truncate") != 0) {
      printf("Truncate : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Truncate : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to truncate
  obj_send.set_path(path);
  obj_send.set_size(newsize);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Truncate : [Write path and newsize] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Truncate : [Read ret] error\n");
    return -1;
  }


  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    return 0;
  }
  else {
    return -1;
  }

  /*
  //log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);
  bb_fullpath(fpath, path);

  return log_syscall("truncate", truncate(fpath, newsize), 0);
  */
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int bb_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);
    bb_fullpath(fpath, path);

    return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int bb_open(const char *path, struct fuse_file_info *fi)
{
  int retstat = 0;
  int fd;
  char fpath[PATH_MAX];
    
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::open obj_send, obj_recv;

  //log_msg("\nbb_open(path\"%s\", fi=0x%08x)\n", path, fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Open : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Open : can’t connect to server \n");
    return -1;
  }

  /////// Send open request to server.
  if (::write(sockfd, "open", 4) < 4) {
    printf("Open : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-open") != 0) {
      printf("Open : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Open : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to open
  obj_send.set_path(path);
  obj_send.set_mode(fi->flags);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Open : [Write path and mode] error\n");
    return -1;
  }

  ////// Receive the fd return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Open : [Read fd] error\n");
    return -1;
  }

  ::close(sockfd);
  
  pthread_mutex_unlock(&mutex_sock);

  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    fi->fh = obj_recv.fd();

    //log_fi(fi);
    return retstat;
  }
  else {
    fi->fh = -1;

    //log_fi(fi);
    return -1;
  }


  /*
  //log_msg("\nbb_open(path\"%s\", fi=0x%08x)\n", path, fi);
  bb_fullpath(fpath, path);

  // if the open call succeeds, my retstat is the file descriptor,
  // else it's -errno.  I'm making sure that in that case the saved
  // file descriptor is exactly -1.
  fd = log_syscall("open", open(fpath, fi->flags), 0);
  if (fd < 0)
    retstat = log_error("open");
	
  fi->fh = fd;

  //log_fi(fi);
    
  return retstat;
  */
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusebb code which returns the amount of data also
// returned by read.
int bb_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  int retstat = 0;
  
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE_WORK] = {0,};
  string str_send, str_recv;
  FormatTransfer::read_write obj_send, obj_recv;
  size_t size_work;
  off_t offset_work;
  //int summ = 0;

  //log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
  //log_fi(fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Read : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Read : can’t connect to server \n");
    return -1;
  }

  /*cout << "[read 1] size : " << size;
  cout << ", offset : " << offset << ", sockfd : " << sockfd << endl;*/

  /////// Send read request to server.
  if (::write(sockfd, "read", 4) < 4) {
    printf("Read : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-read") != 0) {
      printf("Read : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Read : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE_WORK);

  ////// Send the path to open
  obj_send.set_fd(fi->fh);
  obj_send.set_size(size);
  obj_send.set_offset(offset);

  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Read : [Write fd and size and offset] error\n");
    return -1;
  }

  size_work = size;
  offset_work = 0;
  while (size_work > 0) {
    if (::read(sockfd, recv_buf, BLOCK_SIZE_WORK) <= 0) {
      printf("Read : [Read read bytes and buffer] error\n");
      return -1;
    }

    str_recv = recv_buf;
    //cout << "In Read1, length : " << str_recv.length() << endl;
    obj_recv.ParseFromString(str_recv);
    /*cout << "In Read, buf : " << endl << obj_recv.buffer() << endl;
    cout << "And In Read, ret : " << obj_recv.ret();
    cout << ", tid : " << pthread_self() << endl;*/

    ::memcpy(buf + offset_work, obj_recv.buffer().c_str(), BLOCK_SIZE);
    size_work -= BLOCK_SIZE;
    offset_work += BLOCK_SIZE;

    retstat += obj_recv.ret();  ////// To make return value

    ::write(sockfd, "ACK", 3);
    ::memset (recv_buf, 0, BLOCK_SIZE_WORK);
  }

  //cout << "[read final] retstat is " << retstat << endl;
  //cout << "and buf is : " << buf << endl;
  /*for (int i=0;i<size;i++) {
    //printf("%c",buf[i]);
    if (buf[i] == 'a' || buf[i] == '\n')
      summ ++;
  }*/

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);

  //cout << "sum is " << summ << endl;

  return retstat;

  /*
  //log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
  // no need to get fpath on this one, since I work from fi->fh not the path
  //log_fi(fi);

  return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
  */
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int bb_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
  int retstat = 0;

  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE_WORK] = {0,}, wrt_buf[BLOCK_SIZE_WORK];
  string str_send, str_recv;
  FormatTransfer::read_write obj_send, obj_recv;
  size_t size_work;
  off_t offset_work;

  /*for (int i=0;i<size;i++)
    printf("%c",buf[i]);*/
  //log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
  //log_fi(fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Write : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Write : can’t connect to server \n");
    return -1;
  }

  /*cout << "[write 1] size : " << size;
  cout << ", offset : " << offset << ", sockfd : " << sockfd << endl;*/

  /////// Send write request to server.
  if (::write(sockfd, "write", 5) < 5) {
    printf("Write : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-write") != 0) {
      printf("Write : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Write : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE_WORK);

  ////// Send the path to open

  ///////// Repaired because of server's socket size limitation...
  size_work = size;
  offset_work = offset;

  while (size_work > 0) {
    
    ::memcpy(wrt_buf, buf, BLOCK_SIZE);

    obj_send.set_fd(fi->fh);
    obj_send.set_buffer(wrt_buf);
    obj_send.set_size(BLOCK_SIZE);
    obj_send.set_offset(offset_work);

    obj_send.SerializeToString(&str_send);
  //cout << "Here, size of string :" << str_send.length() << endl;
    if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
      printf("Write : [Write fd and buffer and size and offset] error\n");
      return -1;
    }

    if (::read(sockfd, recv_buf, BLOCK_SIZE_WORK) <= 0) {
      printf("Write : [Read written bytes] error\n");
      return -1;
    }
    str_recv = recv_buf;
    obj_recv.ParseFromString(str_recv);

    retstat += obj_recv.ret();

    ::memset(wrt_buf, 0, BLOCK_SIZE_WORK);
    size_work -= BLOCK_SIZE;
    offset_work += BLOCK_SIZE;
  }

  obj_send.set_offset(-323);
  obj_send.SerializeToString(&str_send);
  ::write(sockfd, str_send.c_str(), str_send.length());

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  /*
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  retstat = obj_recv.ret();
  */

  //cout << "[write 2] retstat is " << retstat << endl;

  return retstat;
  
  /*
  //log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
  // no need to get fpath on this one, since I work from fi->fh not the path
  //log_fi(fi);

  return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
  */
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int bb_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_statfs(path=\"%s\", statv=0x%08x)\n", path, statv);
    bb_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    
    //log_statvfs(statv);
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
int bb_flush(const char *path, struct fuse_file_info *fi)
{
    //log_msg("\nbb_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    //log_fi(fi);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int bb_release(const char *path, struct fuse_file_info *fi)
{
  //log_msg("\nbb_release(path=\"%s\", fi=0x%08x)\n", path, fi);
  //log_fi(fi);

  // We need to close the file.  Had we allocated any resources
  // (buffers etc) we'd need to free them here as well.

  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::release obj_send, obj_recv;

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Release : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Release : can’t connect to server \n");
    return -1;
  }

  /////// Send release request to server.
  if (::write(sockfd, "release", 7) < 7) {
    printf("Release : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-release") != 0) {
      printf("Release : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Release : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  /////// Send fd to release
  //obj_send.set_path(path);
  obj_send.set_fd(fi->fh);
  obj_send.SerializeToString(&str_send);

  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Release : [Write path] error\n");
    return -1;
  }

  ////// Receive the return value of release formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Release : [Read stat] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  return obj_recv.ret();

  //return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int bb_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::fsync obj_send, obj_recv;

  //log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
  //log_fi(fi);

#ifdef HAVE_FDATASYNC
  if (datasync)
    return log_syscall("fdatasync", fdatasync(fi->fh), 0);
#endif	

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Fsync : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Fsync : can’t connect to server \n");
    return -1;
  }

  /////// Send fsync request to server.
  if (::write(sockfd, "fsync", 5) < 5) {
    printf("Fsync : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-fsync") != 0) {
      printf("Fsync : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Fsync : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the fd to fsync
  obj_send.set_fd(fi->fh);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Fsync : [Write fd] error\n");
    return -1;
  }

  ////// Receive the return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Fsync : [Read ret] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret()) {
    return 0;
  }
  else {
    return -1;
  }

  /*
  //log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
  //log_fi(fi);
    
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	return log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif	
	return log_syscall("fsync", fsync(fi->fh), 0);
  */
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int bb_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
    bb_fullpath(fpath, path);

    return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int bb_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n", path, name, value, size);
    bb_fullpath(fpath, path);

    retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
    if (retstat >= 0)
	//log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int bb_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    
    //log_msg("bb_listxattr(path=\"%s\", list=0x%08x, size=%d)\n", path, list, size);
    bb_fullpath(fpath, path);

    retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
    if (retstat >= 0) {
	//log_msg("    returned attributes (length %d):\n", retstat);
	for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
	    //log_msg("    \"%s\"\n", ptr);
    }
    
    return retstat;
}

/** Remove extended attributes */
int bb_removexattr(const char *path, const char *name)
{
    char fpath[PATH_MAX];
    
    //log_msg("\nbb_removexattr(path=\"%s\", name=\"%s\")\n", path, name);
    bb_fullpath(fpath, path);

    return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int bb_opendir(const char *path, struct fuse_file_info *fi)
{
  DIR *dp;
  int retstat = 0;
  char fpath[PATH_MAX];
  //int i;
  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::opendir obj_send, obj_recv;

  //log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Opendir : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Opendir : can’t connect to server \n");
    return -1;
  }

  /*i = 0;
  while (1) {
    if (sckbsk[i].available) {
      sockfd = sckbsk[i].sfd;
      sckbsk[i].available = false;
      break;
    }
    i = (i+1)%NUM_THREADS;
  }*/

  //pthread_mutex_lock(&mutex_sock);
  /////// Send opendir request to server.
  if (::write(sockfd, "opendir", 7) < 7) {
    printf("Opendir : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-opendir") != 0) {
      printf("Opendir : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Opendir : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to open (It maybe directory)
  obj_send.set_path(path);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Opendir : [Write path] error\n");
    return -1;
  }

  ////// Receive the DIR* return value formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Opendir : [Read DIR*] error\n");
    return -1;
  }

  ::close(sockfd);
  //sckbsk[i].available = true;
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  //log_msg("    opendir returned 0x%p\n", obj_recv.fd());

  if (obj_recv.ret()) {
    fi->fh = (intptr_t) obj_recv.fd();

    //log_fi(fi);
    return retstat;
  }
  else {
    return -1;
  }

  
  //bb_fullpath(fpath, path);

  // since opendir returns a pointer, takes some custom handling of
  // return status.
  //dp = opendir(fpath);

  //if (dp == NULL)
  //  retstat = log_error("bb_opendir opendir");
    
  //fi->fh = (intptr_t) dp;
    
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int bb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
  int retstat = 0;
  DIR *dp;
  struct dirent *de;

  int sockfd;
  //int i;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::readdir obj_send, obj_recv;

  //log_msg("\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Readdir : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Readdir : can’t connect to server \n");
    return -1;
  }

  /*i = 0;
  while (1) {
    if (sckbsk[i].available) {
      sockfd = sckbsk[i].sfd;
      sckbsk[i].available = false;
      break;
    }
    i = (i+1)%NUM_THREADS;
  }*/

  //pthread_mutex_lock(&mutex_sock);

  if (::write(sockfd, "readdir", 7) < 7) {
    printf("Readdir : [request] error\n");
    return -1;
  }

  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-readdir") != 0) {
      printf("Readdir : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Readdir : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to read (it is maybe directory)
  obj_send.set_path(path);
  obj_send.set_retentry(1);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Readdir : [Write path] error\n");
    return -1;
  }

  ////// Loop for calling filler function on each filename.
  while (1) {
    if ( ::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
      printf("Readdir : [Read directory] error\n");
      return -1; 
    }

    str_recv = recv_buf;
    obj_recv.ParseFromString(str_recv);

    /*if (obj_recv.retentry() != 1) {
      printf("Readdir : [No directory] Server side error\n");
	    retstat = log_error("bb_readdir readdir");
      return obj_recv.retentry();
    }*/

    if (!obj_recv.end()) {
      
      //log_msg("calling filler with name %s\n", obj_recv.filename().c_str());
      if (filler (buf, obj_recv.filename().c_str(), NULL, 0) != 0) {
        return -ENOMEM;
      }

      ///// IMPORTANT.
      ///// It doesn't work when there is no ACK writing!
      ::write (sockfd, "ACK", 3);

    }
    else {
      break;
    }
    ::memset(recv_buf, 0, BLOCK_SIZE);
    str_recv = "";

  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  //sckbsk[i].available = true;

  //pthread_mutex_unlock(&mutex_sock);
  //log_fi(fi);

  return retstat;

  /*
  // once again, no need for fullpath -- but note that I need to cast fi->fh
  dp = (DIR *) (uintptr_t) fi->fh;

  // Every directory contains at least two entries: . and ..  If my
  // first call to the system readdir() returns NULL I've got an
  // error; near as I can tell, that's the only condition under
  // which I can get an error from readdir()
  de = readdir(dp);
  //log_msg("    readdir returned 0x%p\n", de);
  if (de == 0) {
	  retstat = log_error("bb_readdir readdir");
	  return retstat;
  }

  // This will copy the entire directory into the buffer.  The loop exits
  // when either the system readdir() returns NULL, or filler()
  // returns something non-zero.  The first case just means I've
  // read the whole directory; the second means the buffer is full.
  do {
    //log_msg("calling filler with name %s\n", de->d_name);
    if (filler(buf, de->d_name, NULL, 0) != 0) {
    //log_msg("    ERROR bb_readdir filler:  buffer full");
      return -ENOMEM;
    }
  } while ((de = readdir(dp)) != NULL);
    
  //log_fi(fi);
  */
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int bb_releasedir(const char *path, struct fuse_file_info *fi)
{
  int retstat = 0;

  int sockfd;
  //int i;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::release obj_send, obj_recv;

  //log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
  //log_fi(fi);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Releasedir : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Releasedir : can’t connect to server \n");
    return -1;
  }
  /*i = 0;
  while (1) {
    if (sckbsk[i].available) {
      sockfd = sckbsk[i].sfd;
      sckbsk[i].available = false;
      break;
    }
    i = (i+1)%NUM_THREADS;
  }*/

  //pthread_mutex_lock(&mutex_sock);

  /////// Send releasedir request to server.
  if (::write(sockfd, "releasedir", 10) < 10) {
    printf("Releasedir : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-releasedir") != 0) {
      printf("Releasedir : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Releasedir : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  /////// Send fd to releasedir
  //obj_send.set_path(path);
  obj_send.set_fd(fi->fh);
  obj_send.SerializeToString(&str_send);

  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Releasedir : [Write path] error\n");
    return -1;
  }

  ////// Receive the return value of releasedir formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Releasedir : [Read return value] error\n");
    return -1;
  }

  ::close(sockfd);
  pthread_mutex_unlock(&mutex_sock);
  //sckbsk[i].available = true;

  //pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  return obj_recv.ret();

  /*
  //log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
  //log_fi(fi);
    
  closedir((DIR *) (uintptr_t) fi->fh);
    
  return retstat;
  */
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int bb_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    //log_msg("\nbb_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
    //log_fi(fi);
    
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *bb_init(struct fuse_conn_info *conn)
{
  //log_msg("\nbb_init()\n");
    
  log_conn(conn);
  log_fuse_context(fuse_get_context());
  
  return BB_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void bb_destroy(void *userdata)
{
    //log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int bb_access(const char *path, int mask)
{
  int retstat = 0;
  char fpath[PATH_MAX];
  
  int sockfd;
  //int i;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::access obj_send, obj_recv;

  //log_msg("\nbb_access(path=\"%s\", mask=0%o)\n", path, mask);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Access : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Access : can’t connect to server \n");
    return -1;
  }
  /*i = 0;
  while (1) {
    if (sckbsk[i].available) {
      sockfd = sckbsk[i].sfd;
      sckbsk[i].available = false;
      break;
    }
    i = (i+1)%NUM_THREADS;
  }*/

  //pthread_mutex_lock(&mutex_sock);

  /////// Send getattr request to server.
  if (::write(sockfd, "access", 6) < 6) {
    printf("Access : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-access") != 0) {
      printf("Access : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Access : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to get attributes
  obj_send.set_path(path);
  obj_send.set_mask(mask);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Access : [Write path] error\n");
    return -1;
  }

  ////// Receive the return value of releasedir formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Access : [Access return value] error\n");
    return -1;
  }

  ::close(sockfd);
  //sckbsk[i].available = true;
  pthread_mutex_unlock(&mutex_sock);
  
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret())
    return retstat;
  else 
    return -1;

  /*
  //log_msg("\nbb_access(path=\"%s\", mask=0%o)\n", path, mask);
  bb_fullpath(fpath, path);
    
  retstat = access(fpath, mask);
    
  if (retstat < 0)
    retstat = log_error("bb_access access");
  
  return retstat;
  */
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int bb_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
  int retstat = 0;
    
  //log_msg("\nbb_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n", path, offset, fi);
  //log_fi(fi);
    
  retstat = ftruncate(fi->fh, offset);
  if (retstat < 0)
    retstat = log_error("bb_ftruncate ftruncate");
    
  return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int bb_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
  int retstat = 0;

  int sockfd;
  struct sockaddr_in serv_addr;
  char buff[BLOCK_SIZE], recv_buf[BLOCK_SIZE] = {0,};
  string str_send, str_recv;
  FormatTransfer::fgetattr obj_send, obj_recv;

  //log_msg("\nbb_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);
  //log_fi(fi);

  if (!strcmp(path, "/"))
    return bb_getattr(path, statbuf);

  ::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);

  pthread_mutex_lock(&mutex_sock);

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Fgetattr : can’t open stream socket\n");
    return -1;
  }
  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Fgetattr : can’t connect to server \n");
    return -1;
  }

  /////// Send getattr request to server.
  if (::write(sockfd, "fgetattr", 8) < 8) {
    printf("Fgetattr : [Request] error\n");
    return -1;
  }

  ////// Read ACK from server.
  if (::read(sockfd, recv_buf, BLOCK_SIZE) > 0) {
    if (strcmp(recv_buf, "ACK-fgetattr") != 0) {
      printf("Fgetattr : [Wrong ACK] error\n");
      return -1;
    }
  }
  else {
    printf("Fgetattr : [None ACK] error\n");
    return -1;
  }

  ::memset(recv_buf, 0, BLOCK_SIZE);

  ////// Send the path to get attributes
  obj_send.set_fd(fi->fh);
  obj_send.SerializeToString(&str_send);
  if (::write(sockfd, str_send.c_str(), str_send.length()) < str_send.length()) {
    printf("Fgetattr : [Write path] error\n");
    return -1;
  }

  ////// Receive the attributes information formatted google protobuf
  if (::read(sockfd, recv_buf, BLOCK_SIZE) <= 0) {
    printf("Fgetattr : [Read stat] error\n");
    printf("path : %s\n", path);
    return -1;
  }

  ////// Parse the FormatTransfer::getattr object to stat structure
  str_recv = recv_buf;
  obj_recv.ParseFromString(str_recv);

  if (obj_recv.ret() == 0) {

    statbuf->st_dev = obj_recv.dev();
    statbuf->st_ino = obj_recv.inode();
    statbuf->st_mode = obj_recv.mode();
    statbuf->st_nlink = obj_recv.nlink();
    statbuf->st_uid = obj_recv.uid();
    statbuf->st_gid = obj_recv.gid();

    ////// Because of this, I got a problem!!!!!
    //statbuf->st_rdev = obj_recv.devid();
    
    statbuf->st_size = obj_recv.size();
    statbuf->st_blksize = obj_recv.blksize();
    statbuf->st_blocks = obj_recv.nblk();
    statbuf->st_atime = obj_recv.atime();
    statbuf->st_mtime = obj_recv.mtime();
    statbuf->st_ctime = obj_recv.ctime();
    retstat = obj_recv.ret();
  }
  else
    retstat = -1;

  ::close(sockfd);

  pthread_mutex_unlock(&mutex_sock);
  return retstat;

  // On FreeBSD, trying to do anything with the mountpoint ends up
  // opening it, and then using the FD for an fgetattr.  So in the
  // special case of a path of "/", I need to do a getattr on the
  // underlying root directory instead of doing the fgetattr().
    
  /*
  retstat = fstat(fi->fh, statbuf);
  if (retstat < 0)
    retstat = log_error("bb_fgetattr fstat");
    
  //log_stat(statbuf);
    
  return retstat;
  */
}

/*struct bb_oper : fuse_operations
{
  fms_operations() {
    .getattr = bb_getattr,
    .readlink = bb_readlink,
    // no .getdir -- that's deprecated
    .getdir = NULL,
    .mknod = bb_mknod,
    .mkdir = bb_mkdir,
    .unlink = bb_unlink,
    .rmdir = bb_rmdir,
    .symlink = bb_symlink,
    .rename = bb_rename,
    .link = bb_link,
    .chmod = bb_chmod,
    .chown = bb_chown,
    .truncate = bb_truncate,
    .utime = bb_utime,
    .open = bb_open,
    .read = bb_read,
    .write = bb_write,
    .statfs = bb_statfs,
    .flush = bb_flush,
    .release = bb_release,
    .fsync = bb_fsync,
    
  #ifdef HAVE_SYS_XATTR_H
    .setxattr = bb_setxattr,
    .getxattr = bb_getxattr,
    .listxattr = bb_listxattr,
    .removexattr = bb_removexattr,
  #endif
    
    .opendir = bb_opendir,
    .readdir = bb_readdir,
    .releasedir = bb_releasedir,
    .fsyncdir = bb_fsyncdir,
    .init = bb_init,
    .destroy = bb_destroy,
    .access = bb_access,
    .ftruncate = bb_ftruncate,
    .fgetattr = bb_fgetattr
  }
} bb_oper_init;*/

void bb_usage()
{
  fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
  abort();
}

int main(int argc, char *argv[])
{
  int fuse_stat;
  struct bb_state *bb_data;
  //int sockfd;
  struct sockaddr_in serv_addr;

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  // bbfs doesn't do any access checking on its own (the comment
  // blocks in fuse.h mention some of the functions that need
  // accesses checked -- but note there are other functions, like
  // chown(), that also need checking!).  Since running bbfs as root
  // will therefore open Metrodome-sized holes in the system
  // security, we'll check if root is trying to mount the filesystem
  // and refuse if it is.  The somewhat smaller hole of an ordinary
  // user doing it with the allow_other flag is still there because
  // I don't want to parse the options string.
  if ((getuid() == 0) || (geteuid() == 0)) {
    fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
    return 1;
  }

  // See which version of fuse we're running
  fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
  // Perform some sanity checking on the command line:  make sure
  // there are enough arguments, and that neither of the last two
  // start with a hyphen (this will break if you actually have a
  // rootpoint or mountpoint whose name starts with a hyphen, but so
  // will a zillion other programs)
  if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	  bb_usage();

  bb_data = (struct bb_state*)malloc(sizeof(struct bb_state));
  //bb_data = new struct bb_state;
  if (bb_data == NULL) {
    perror("main calloc");
    abort();
  }

  pthread_mutex_init(&mutex_sock, NULL);

  // Pull the rootdir out of the argument list and save it in my
  // internal data
  bb_data->dir_ssd = realpath(argv[argc-2], NULL);
  argv[argc-2] = argv[argc-1];
  argv[argc-1] = NULL;
  argc--;
    
  bb_data->logfile = log_open();
    
  bb_oper.init = bb_init;
  bb_oper.getattr = bb_getattr;
  bb_oper.fgetattr = bb_fgetattr;
  bb_oper.access = bb_access;
  bb_oper.readlink = bb_readlink;
  bb_oper.readdir = bb_readdir;
  bb_oper.mknod = bb_mknod;
  bb_oper.mkdir = bb_mkdir;
  bb_oper.symlink = bb_symlink;
  bb_oper.unlink = bb_unlink;
  bb_oper.rmdir = bb_rmdir;
  bb_oper.rename = bb_rename;
  bb_oper.link = bb_link;
  bb_oper.chmod = bb_chmod;
  bb_oper.chown = bb_chown;
  bb_oper.flush = bb_flush;
  bb_oper.truncate = bb_truncate;
  bb_oper.ftruncate = bb_ftruncate;
  bb_oper.utime = bb_utime;
  bb_oper.open = bb_open;
  bb_oper.opendir = bb_opendir;
  bb_oper.read = bb_read;
  bb_oper.write = bb_write;
  bb_oper.destroy = bb_destroy;
  bb_oper.statfs = bb_statfs;
  bb_oper.release = bb_release;
  bb_oper.releasedir = bb_releasedir;
  bb_oper.fsync = bb_fsync;
  bb_oper.fsyncdir = bb_fsyncdir;
#ifdef HAVE_SYS_XATTR_H
  bb_oper.setxattr = bb_setxattr;
  bb_oper.getxattr = bb_getxattr;
  bb_oper.listxattr = bb_listxattr;
  bb_oper.removexattr = bb_removexattr;
#endif

  //pthread_mutex_lock(&temp);
  /*::memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
  serv_addr.sin_port = htons(SERV_TCP_PORT);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    printf("Main : can’t open stream socket\n");
    return NULL;
  }

  if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    printf("Main : can’t connect to server\n");
    return NULL;
  }*/

  /*for (int i=0; i < NUM_THREADS; i++)
  {  
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
      printf("Init : can’t open stream socket\n");
      return NULL;
    }
    if (::connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      printf("Init : can’t connect to server of %d socket\n", i);
      return NULL;
    }
   
    sckbsk[i].sfd = sockfd;
    sckbsk[i].available = true;
  }
  pthread_mutex_unlock(&temp);*/

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  //pthread_mutex_lock(&mutex_sock);
  fuse_stat = fuse_main(argc, argv, &bb_oper, bb_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
  return fuse_stat;
}
