#include <cjson/cJSON.h>
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 30
#endif
#include <fuse.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>


cJSON *MAIN_JSON_OBJECT;
char* filename = NULL;

cJSON *path_to_json(const char* p)
{
        char* path = malloc((strlen(p) + 1)*sizeof(char));
        memcpy(path, p, strlen(p) + 1);

        
        if(strcmp("/", path) == 0)
        {
                // parent path
                return MAIN_JSON_OBJECT;
        }
        

        const char delim[2] = "/";
        char* token = NULL;

        token = strtok(path, delim);
        
        int invalidpath = 0;
        cJSON *parent =  MAIN_JSON_OBJECT;
        cJSON *leaf = NULL;
        while(token != NULL)
        {
                
                leaf = cJSON_GetObjectItemCaseSensitive(parent, token);
                if(leaf == NULL)
                {
                        printf("invalid!\n");
                        invalidpath = 1;
                        break;
                }
                parent = leaf;
                token = strtok(NULL, delim);
        }

        free(path);
        return leaf;
}

static int fun_getattr(const char *path, struct stat *stbuf)
{

	int fd = open(filename, O_RDONLY);
        int len = lseek(fd, 0, SEEK_END);
        char* jsonstring = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
        MAIN_JSON_OBJECT = cJSON_Parse(jsonstring);
        printf("%s\n", cJSON_Print(MAIN_JSON_OBJECT));
        close(fd);


        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = time(NULL);

        cJSON* obj = path_to_json(path);
        if(obj == NULL)
        {
                printf("getattr invalid path!: %s\n", path);
                return -1;
        }

        if(strcmp("/", path) == 0)
        {
                // root
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                return 0;
        }

        if(cJSON_IsObject(obj))
        {
                // this is a directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 1;
                return 0;
        }
        else
        {
                // this is a regular file
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = strlen(obj->valuestring);
                return 0;
        }

        printf("getattr error!\n");
        return -1;
}

//  will be executed when the system asks for a list of files that were stored in the mount point
static int fun_readdir(const char *path, void *buff, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
        //Bash sends "/.Trash" or "autorun.inf" life paths by itself.
        //These are excluded from our project.
        

        filler(buff, ".", NULL, 0);  //Current directory
        filler(buff, "..", NULL, 0); //Parent directory

        cJSON *obj = path_to_json(path);

        if(obj == NULL)
        {
                printf("readdir invalid path!: %s\n", path);
                return -1;
        }

        cJSON *iter;
        cJSON_ArrayForEach(iter, obj)
        {
                filler(buff, iter->string, NULL, 0);
        }
        
        return 0;
}

static int fun_read(const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi)
{

        cJSON *obj = path_to_json(path);

        if(obj == NULL)
        {
                printf("read invalid path!: %s\n", path);
        }

        if(cJSON_IsObject(obj))
        {
                printf("trying to read a directory!: %s\n", path);
                return -1;
        }

        size_t len = strlen(obj->valuestring);
        if(offset < len)
        {
                if(offset + size > len)
                        size = len - offset;
                memcpy(buff, obj->valuestring + offset, size);
        }
        else
        {
                size = 0;
        }

        return size;
}

static int fun_open(const char *path, struct fuse_file_info *fi)
{
        cJSON *obj = path_to_json(path);

        if(obj == NULL)
        {
                printf("open invalid path!: %s", path);
                return -1;
        }

        if(cJSON_IsObject(obj))
        {
                printf("%s\n", cJSON_Print(obj));
                // this should not be a directory
                return -1;
        }

        return 0;
}

static int fun_poll(const char * path, struct fuse_file_info *fi, struct fuse_pollhandle *ph, unsigned *reventsp)
{
	printf("polling??\n");
}

static struct fuse_operations operations = {
    .getattr    = fun_getattr,
    .readdir    = fun_readdir,
    .read       = fun_read,
    .open       = fun_open,
    .poll	= fun_poll
};

int main(int argc, char** argv)
{
        int pos_json = argc - 1;
        int newsize = argc - 1;
	filename = argv[pos_json];	


        char** fuse_args = malloc(newsize * sizeof(char*));
        for(int i = 0; i < newsize; i++)
                fuse_args[i] = argv[i];

        int fuse_result = fuse_main(newsize, fuse_args, &operations, NULL);

        free(fuse_args);
        cJSON_Delete(MAIN_JSON_OBJECT);
        
        return fuse_result;
}