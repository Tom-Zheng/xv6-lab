#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int TestFileName(const char* path, const char* name) {
  // find start of filename
  const char* p = path + strlen(path);
  while (p >= path && *p != '/') {--p;}
  ++p;
  return !strcmp(p, name);
}

int IsDir(const char* path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    return -1;
  }
  return st.type == T_DIR;
}

int _find_dir(const char* path, const char* name) {
  // printf("DEBUG: _find_dir(%s, %s)\n", path, name);
  
  int fd;
  struct stat st;
  struct dirent de;
  char buf[500];
  char* p;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return -1;
  }
  
  if (TestFileName(path, name)) {
    printf("%s\n",path);
  }
  
  strcpy(buf, path);
  p = buf+strlen(buf);
  *p++ = '/';
  // printf("DEBUG: init buf: %s\n", buf);

  
  if (strlen(buf) + 1 + DIRSIZ + 1 > 500) {
    fprintf(2, "path too long\n");
    return -1;
  }
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    p[DIRSIZ] = 0;
    memcpy(p, de.name, DIRSIZ);
    // printf("DEBUG: buf: %s\n", buf);
    if(de.inum == 0)
      continue;
    if (!strcmp(p, ".") || !strcmp(p, ".."))
      continue;

    
    struct stat st_entry;
    if (stat(buf, &st_entry) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      return -1;
    }
    if (st_entry.type == T_DIR) {
      if (_find_dir(buf, name) < 0)
        return -1;
    } else if (TestFileName(buf, name)) {
      printf("%s\n", buf);
    }
  }
  return 0;
}

int _find(const char* path, const char* name) {
  // printf("DEBUG: _find(%s, %s)\n", path, name);
  int status = IsDir(path);
  if (status == -1) {
    return -1;
  }
  else if (status == 0) {
    if (TestFileName(path, name)) {
      printf("%s\n",path);
    }
    return 0;
  } else {
    return _find_dir(path, name);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    int status = _find(".", argv[1]);
    exit(status);
  } else if (argc == 3) {
    int status = _find(argv[1], argv[2]);
    exit(status);
  } else {
    printf("find [path] [filename]\n");
    exit(-1);
  }
  exit(0);
}
