#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/time.h>

#define BUFFER_SIZE 6000

void rec_rmdir(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char fullpath[BUFFER_SIZE];

    dir = opendir(path);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullpath, BUFFER_SIZE, "%s/%s", path, entry->d_name);
        if (lstat(fullpath, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                rec_rmdir(fullpath);
                printf("[-] %s\n", fullpath);
            } else {
                remove(fullpath);
                printf("[-] %s\n", fullpath);
            }
        }
    }

    closedir(dir);
    rmdir(path);
}

void sync_directories(const char *src_dir, const char *dst_dir) {
    DIR *src_dir_ptr, *dst_dir_ptr;
    struct dirent *src_dirent, *dst_dirent;
    struct stat src_stat, dst_stat;
    char src_path[BUFFER_SIZE], dst_path[BUFFER_SIZE];
    int src_fd, dst_fd;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;

    // Open source and destination directories
    src_dir_ptr = opendir(src_dir);
    dst_dir_ptr = opendir(dst_dir);

    // Synchronize files and directories
    while ((src_dirent = readdir(src_dir_ptr)) != NULL) {
        if (strcmp(src_dirent->d_name, ".") == 0 || strcmp(src_dirent->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, BUFFER_SIZE, "%s/%s", src_dir, src_dirent->d_name);
        snprintf(dst_path, BUFFER_SIZE, "%s/%s", dst_dir, src_dirent->d_name);

        // Check if the item is a regular file
        if (stat(src_path, &src_stat) == 0 && S_ISREG(src_stat.st_mode)) {
            // Case 1: item is a regular file
            if (stat(dst_path, &dst_stat) != 0) {
                // dst_path/item does not exist, copy src_path/item to dst_path/item
                printf("[+] %s\n", dst_path);
                src_fd = open(src_path, O_RDONLY);
                dst_fd = creat(dst_path, src_stat.st_mode);
                while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
                    bytes_written = write(dst_fd, buffer, bytes_read);
                    if (bytes_written != bytes_read) {
                        fprintf(stderr, "Error writing to %s\n", dst_path);
                        break;
                    }
                }
                close(src_fd);
                close(dst_fd);
            } else if (src_stat.st_size != dst_stat.st_size || src_stat.st_mtime != dst_stat.st_mtime) {
                // src_path/item and dst_path/item exist, but are different, copy src_path/item to dst_path/item
                printf("[o] %s\n", dst_path);
                src_fd = open(src_path, O_RDONLY);
                dst_fd = open(dst_path, O_WRONLY | O_TRUNC);
                while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
                    bytes_written = write(dst_fd, buffer, bytes_read);
                    if (bytes_written != bytes_read) {
                        fprintf(stderr, "Error writing to %s\n", dst_path);
                        break;
                    }
                }
                close(src_fd);
                close(dst_fd);
            }
        } else {
            // Case 2: item is a directory
            if (stat(dst_path, &dst_stat) != 0) {
                // dst_path/item does not exist, recursively copy src_path/item to dst_path/item
                printf("[+] %s\n", dst_path);
                mkdir(dst_path, src_stat.st_mode);
                sync_directories(src_path, dst_path);
            } else if (S_ISDIR(dst_stat.st_mode)) {
                // Both src_path/item and dst_path/item exist and are directories, recurse into them
                sync_directories(src_path, dst_path);
            } else {
                // dst_path/item exists but is not a directory, delete it
                printf("[-] %s\n", dst_path);
                remove(dst_path);
                mkdir(dst_path, src_stat.st_mode);
                sync_directories(src_path, dst_path);
            }
        }
    }

    // Synchronize timestamps and permissions
    rewinddir(dst_dir_ptr);
    while ((dst_dirent = readdir(dst_dir_ptr)) != NULL) {
        if (strcmp(dst_dirent->d_name, ".") == 0 || strcmp(dst_dirent->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, BUFFER_SIZE, "%s/%s", src_dir, dst_dirent->d_name);
        snprintf(dst_path, BUFFER_SIZE, "%s/%s", dst_dir, dst_dirent->d_name);

        if (stat(src_path, &src_stat) == 0) {
            // src_path/item exists is a regular file or directory
            if (stat(dst_path, &dst_stat) == 0) {
                if (src_stat.st_mtime != dst_stat.st_mtime || src_stat.st_atime != dst_stat.st_atime) {
                    printf("[t] %s\n", dst_path);

                    struct timespec times[2] = {
                        {.tv_sec = src_stat.st_atime, .tv_nsec = 0},
                        {.tv_sec = src_stat.st_mtime, .tv_nsec = 0},
                    };

                    if (utimensat(AT_FDCWD, dst_path, times, AT_SYMLINK_NOFOLLOW) != 0) {
                        perror("utimensat");
                    }
                }
                if (src_stat.st_mode != dst_stat.st_mode) {
                    printf("[p] %s\n", dst_path);
                    chmod(dst_path, src_stat.st_mode);
                }
            }
        } else {
            //Check if file or directory, and remove accordingly
            if (stat(dst_path, &dst_stat) == 0) {
                if (S_ISDIR(dst_stat.st_mode)) {
                    // Recursively remove directory
                    rec_rmdir(dst_path);
                } else {
                    remove(dst_path);
                }
            }
            printf("[-] %s\n", dst_path);
        }
    }

    closedir(src_dir_ptr);
    closedir(dst_dir_ptr);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <src_dir> <dst_dir>\n", argv[0]);
        return 1;
    }

    sync_directories(argv[1], argv[2]);
    return 0;
}