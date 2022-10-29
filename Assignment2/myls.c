#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    DIR *dir;
    struct dirent *ent;

    dir = (argv[1] != NULL) ? opendir(argv[1]) : opendir(".");

    int redirect_fd = open("list.txt", O_CREAT | O_TRUNC | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    dup2(redirect_fd, 1);
    close(redirect_fd);

    while ((ent = readdir(dir)) != NULL)
    {
        struct stat sbuf;

        char prefix[100] = "";
        sprintf(prefix, "%s/%s", argv[1], ent->d_name);
        stat(prefix, &sbuf);

        printf((S_ISDIR(sbuf.st_mode)) ? "d" : "-");

        // Permissions for user
        printf((sbuf.st_mode & S_IRUSR) ? "r" : "-");
        printf((sbuf.st_mode & S_IWUSR) ? "w" : "-");
        printf((sbuf.st_mode & S_IXUSR) ? "x" : "-");

        // Permissions for group
        printf((sbuf.st_mode & S_IRGRP) ? "r" : "-");
        printf((sbuf.st_mode & S_IWGRP) ? "w" : "-");
        printf((sbuf.st_mode & S_IXGRP) ? "x" : "-");

        // Permissions for others
        printf((sbuf.st_mode & S_IROTH) ? "r" : "-");
        printf((sbuf.st_mode & S_IWOTH) ? "w" : "-");
        printf((sbuf.st_mode & S_IXOTH) ? "x" : "-");

        // File size
        printf("\t%d", sbuf.st_size);

        // Last modified date and time
        char *t = ctime(&sbuf.st_ctime);
        if (t[strlen(t) - 1] == '\n')
            t[strlen(t) - 1] = '\0';
        printf("\t%s", t);

        printf("\t%s\n", ent->d_name);
    }

    close((unsigned long)dir);

    return 0;
}