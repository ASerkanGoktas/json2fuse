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
#include <errno.h>

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

cJSON* get_parent(const char* p)
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

        while(1)
        {
                leaf = cJSON_GetObjectItemCaseSensitive(parent, token);
                if(leaf == NULL)
                {
                        invalidpath = 1;
                        break;
                } 

                token = strtok(NULL, delim);
                if(token == NULL)
                {
                        break;
                }
                else
                {
                        parent = leaf;
                }
        }

        free(path);
        return parent;
}

void read_the_structure()
{
        cJSON_Delete(MAIN_JSON_OBJECT);
        MAIN_JSON_OBJECT = NULL;
                // parsing the json file
        jsonfd = open(filename, O_RDWR);
        int len = lseek(jsonfd, 0, SEEK_END);
        jsonstring = mmap(NULL, len, PROT_READ, MAP_PRIVATE, jsonfd, 0);
        MAIN_JSON_OBJECT = cJSON_Parse(jsonstring);
        close(jsonfd);
}

void update_the_structure()
{
        FILE* f = fopen(filename, "w");
        fprintf(f, "%s", cJSON_Print(MAIN_JSON_OBJECT));
        fclose(f);
}


static int fun_getattr(const char *path, struct stat *stbuf)
{
	int ret = -ENOENT;

        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = time(NULL);

        cJSON* obj = path_to_json(path);
        if(strcmp("/", path) == 0)
        {
                read_the_structure();
                // root
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                ret = 0;

        }
        else if(obj == NULL)
        {
                ret = -ENOENT;
        }
        else if(cJSON_IsObject(obj))
        {
                // this is a directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
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
                return -ENOENT;
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
                return -ENOENT;
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
                return -ENOENT;
        }

        if(cJSON_IsObject(obj))
        {
                printf("%s\n", cJSON_Print(obj));
                // this should not be a directory
                return -ENOENT;
        }

        return 0;
}

static int fun_write(const char * path, const char * buff, size_t size, off_t offset, struct fuse_file_info * fi)
{
	cJSON *obj = path_to_json(path);
        if(obj == NULL)
        {
                return -ENOENT;
        }

        if(cJSON_IsObject(obj))
        {
                // this should not be a directory
                return -ENOENT;
        }
	else
	{       
                // free(obj->valuestring); // TODO memory leak
                char* s = calloc(size, sizeof(char));
                memcpy(s, buff, size);
                s[size] = NULL;
                cJSON_SetValuestring(obj, s);
                free(s);
                update_the_structure();
                return size;
	}
        
        return -ENOENT;
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
                cJSON* parent = get_parent(path);
                cJSON_DetachItemViaPointer(parent, obj);
                cJSON_free(obj);
                update_the_structure();
                return 0;
        }

        return -ENOENT;
}

static int fun_mkdir(const char* path, mode_t mode)
{
        char* parentpath = malloc(sizeof(char) * strlen(path));

        char stop = '/';
        int len_directory_name = 0;
        for(int i = strlen(path) - 1; i >= 0; i--)
        {
                if(stop == path[i])
                {
                        break;
                }
                len_directory_name++;
        }
        
        int cpylen = strlen(path) - len_directory_name;

        int i = 0;
        for(; i < cpylen; i++)
        {
                parentpath[i] = path[i];
        }
        parentpath[i] = '\0';
        cJSON *obj = path_to_json(parentpath);

        int indexstart = strlen(path) - len_directory_name;
        cJSON_AddObjectToObject(obj, path + (strlen(path) - len_directory_name));
        update_the_structure();
        free(parentpath);

        return 0;
}

static int fun_create(const char* path, mode_t mode, struct fuse_file_info *fi)
{
        char* parentpath = malloc(sizeof(char) * strlen(path));

        char stop = '/';
        int len_directory_name = 0;
        for(int i = strlen(path) - 1; i >= 0; i--)
        {
                if(stop == path[i])
                {
                        break;
                }
                len_directory_name++;
        }
        
        int cpylen = strlen(path) - len_directory_name;

        int i = 0;
        for(; i < cpylen; i++)
        {
                parentpath[i] = path[i];
        }
        parentpath[i] = '\0';
        cJSON *obj = path_to_json(parentpath);

        int indexstart = strlen(path) - len_directory_name;
        cJSON_AddStringToObject(obj, path + (strlen(path) - len_directory_name), "");
        update_the_structure();
        free(parentpath);

        return 0;
}

static int fun_unlink(const char* path)
{ 
        cJSON *obj = path_to_json(path);
        printf("unlink path: %s", path);
        
        if(obj == NULL)
        {
                printf("open invalid path!: %s", path);
                return -ENOENT;
        }

        if(!cJSON_IsObject(obj))
        {
                cJSON* parent = get_parent(path);
                cJSON_DetachItemViaPointer(parent, obj);
                update_the_structure();
                cJSON_free(obj);
                return 0;
        }

        return -ENOENT;
}

int fun_truncate(const char * path, off_t offset, struct fuse_file_info *fi)
{
        return 0;
}

static int fun_mknod( const char *path, mode_t mode, dev_t rdev )
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
    .mknod      = fun_mknod,
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
