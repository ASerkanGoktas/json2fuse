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


cJSON *MAIN_JSON_OBJECT = NULL;
char* filename = NULL;
char* jsonstring = NULL;
int jsonfd = 0;

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
	int ret = -1;

        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = time(NULL);

        cJSON* obj = path_to_json(path);
        if(strcmp("/", path) == 0)
        {
                // root
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                ret = 0;

        }
        else if(obj == NULL)
        {
                ret = -1;
        }
        else if(cJSON_IsObject(obj))
        {
                // this is a directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 1;
                ret = 0;
        }
        else
        {
                // this is a regular file
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = strlen(obj->valuestring);
                ret = 0;
        }

        return ret;
}

//  will be executed when the system asks for a list of files that were stored in the mount point
static int fun_readdir(const char *path, void *buff, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
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

static int fun_write(const char * path, const char * buff, size_t size, off_t offset, struct fuse_file_info * fi)
{
	cJSON *obj = path_to_json(path);
        if(obj == NULL)
        {
                return -1;
        }

        if(cJSON_IsObject(obj))
        {
                // this should not be a directory
                return -1;
        }
	else
	{       
                // free(obj->valuestring); // TODO memory leak
                char* s = calloc(size, sizeof(char));
                memcpy(s, buff, size);
                s[size] = NULL;
                cJSON_SetValuestring(obj, s);
                free(s);
                return size;
	}
        
        return -1;
}

static int fun_rmdir(const char* path)
{
	cJSON *obj = path_to_json(path);

        if(obj == NULL)
        {
                printf("open invalid path!: %s", path);
        }

        if(cJSON_IsObject(obj))
        {

                printf("You may only delete directories via changing input json file!\n");
        }

        return -1;
}

static int fun_mkdir(const char* path, mode_t mode)
{
        printf("You may only make directories via changing input json file!\n");
        return -1;
}

static int fun_create(const char* path, mode_t mode, struct fuse_file_info *fi)
{
	printf("You may only create files via changing input json file!\n");
        return -1;
}

static int fun_unlink(const char* path)
{ 
        printf("You may only delete files via changing input json file!\n");
        return -1;
}

int fun_truncate(const char * path, off_t offset, struct fuse_file_info *fi)
{
        return 0;
}

static struct fuse_operations operations = {
    .getattr    = fun_getattr,
    .readdir    = fun_readdir,
    .read       = fun_read,
    .open       = fun_open,
    .write	= fun_write,
    .rmdir	= fun_rmdir,
    .mkdir	= fun_mkdir,
    .create	= fun_create,
    .unlink	= fun_unlink,
    .truncate   = fun_truncate,
};

int main(int argc, char** argv)
{
        int pos_json = argc - 1;
        int newsize = argc - 1;
	filename = argv[pos_json];	


        char** fuse_args = malloc(newsize * sizeof(char*));
        for(int i = 0; i < newsize; i++)
                fuse_args[i] = argv[i];


        // parsing the json file
        jsonfd = open(filename, O_RDWR);
        int len = lseek(jsonfd, 0, SEEK_END);
        jsonstring = mmap(NULL, len, PROT_READ, MAP_PRIVATE, jsonfd, 0);
        MAIN_JSON_OBJECT = cJSON_Parse(jsonstring);
        close(jsonfd);

        // fuse main
        int fuse_result = fuse_main(newsize, fuse_args, &operations, NULL);
        printf("filesystem closing...\n");
        
        // saving the changes to the file system
        const char* out = cJSON_Print(MAIN_JSON_OBJECT);
        FILE* f = fopen(filename, "w");
        fprintf(f, "%s", out);
        fclose(f);
        
        // clean up
        free(fuse_args);
        cJSON_Delete(MAIN_JSON_OBJECT);
        
        return fuse_result;
}
