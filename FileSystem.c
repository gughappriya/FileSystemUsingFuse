/*This project is created  and implemented to get good understanding of fuse . The following features are implemented,
 * 1. Implemented as flat file system 
 * 2. Will be able to create directory and file under root node only
 * 3. Will be able to do create,write operations on file.
 * 4. Listing of file and directories is also implemented
 * 5. Mouting and unmounting several times doesnt affect the file system
 */
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include<time.h>
#include<unistd.h>

const int maxblock =10000;
const int blockCount = 25;
const int blocksize = 4096;
int currentDir =26;
char fusedata_path[100] ="../fusedata/fusedata.";
int freeList[25][400];

struct Superblock{
	long creationTime;
	int mounted;
	int devId;
	int freeStart;
	int freeEnd;
	int root;
	int maxBlocks;
};
struct Directory{
	int size;//verify if long
	int uid;
	int gid;
	int mode;
	long atime;
	long ctime;
	long mtime;
	int linkcount;
	char fileToInodeDict[1000];
};
struct File{
	int size;//verify if long
	int uid;
	int gid;
	int mode;
	int linkcount;
	long atime;
	long ctime;
	long mtime;
	int indirect;
	int location;
};

char* setFuseDataPath(){

	char cwd[100];
	getcwd(cwd, sizeof(cwd));
	sprintf(fusedata_path,"%s%s",cwd,"/fusedata/fusedata.");
	return fusedata_path;
}
//Start of Copied from http://www.programmingsimplified.com/c/source-code/c-program-insert-substring-into-string
char *substring(char *string, int position, int length)
{
	char *pointer;
	int c;

	pointer = malloc(length+1);

	if( pointer == NULL )
		exit(EXIT_FAILURE);

	for( c = 0 ; c < length ; c++ )
		*(pointer+c) = *((string+position-1)+c);

	*(pointer+c) = '\0';

	return pointer;
}
void insert_substring(char *a, char *b, int position)
{
	char *f, *e;
	int length;

	length = strlen(a);

	f = substring(a, 1, position - 1 );
	e = substring(a, position, length-position+1);

	strcpy(a, "");
	strcat(a, f);
	free(f);
	strcat(a, b);
	strcat(a, e);
	free(e);
}
//End of code copied from http://www.programmingsimplified.com/c/source-code/c-program-insert-substring-into-string
void initialSetup()
{
	char fileName[100];
	sprintf(fileName,"%s%d",fusedata_path,0);
	if(access(fileName,F_OK)!=-1){
		//Increment mounted count in Super block
		FILE *sbFile = fopen(fileName,"r+w");
		struct Superblock superblock ;
		fscanf(sbFile,"{creationTime:%ld,mounted:%d,devId:%d,freeStart:%d,freeEnd:%d,root:%d,maxBlocks:%d}",&superblock.creationTime,&superblock.mounted,&superblock.devId,&superblock.freeStart,&superblock.freeEnd,&superblock.root,&superblock.maxBlocks);
		superblock.mounted = superblock.mounted+1;
		fseek(sbFile,0,SEEK_SET);
		fprintf(sbFile,"{creationTime:%ld,mounted:%d,devId:%d,freeStart:%d,freeEnd:%d,root:%d,maxBlocks:%d}",superblock.creationTime,superblock.mounted,superblock.devId,superblock.freeStart,superblock.freeEnd,superblock.root,superblock.maxBlocks);
		fclose(sbFile);
		//Get the free block list in global array
		int j;
		for (j=0;j<25;j++){
			int k=0;
			char fileName[100];
			sprintf(fileName,"%s%d",fusedata_path,j+1);
			FILE *fp = fopen(fileName, "r+");
			while(!feof(fp)){
				fscanf(fp,"%d,",&freeList[j][k]);
				k++;
			}
			fclose(fp);
		}

	}else{
		int i=0;
		for(i=0; i<maxblock ; i++){
			int *ch;
			ch = (int*)malloc(blocksize);
			char fileName[100];
			sprintf(fileName,"%s%d",fusedata_path,i);
			FILE *fp = fopen(fileName, "ab+");
			fwrite(ch,blocksize,1,fp);
			fclose(fp);
		}

		//Add content to superblock
		char fileName[100];
		sprintf(fileName,"%s%d",fusedata_path,0);
		FILE *sbFile = fopen(fileName,"w+");
		struct Superblock superblock ;
		superblock.creationTime =(long)time(NULL);
		superblock.mounted = 1;
		superblock.devId = 20;
		superblock.freeStart = 1;
		superblock.freeEnd = 25;
		superblock.root = 26;
		superblock.maxBlocks = 10000;
		fprintf(sbFile,"{creationTime:%ld,mounted:%d,devId:%d,freeStart:%d,freeEnd:%d,root:%d,maxBlocks:%d}",superblock.creationTime,superblock.mounted,superblock.devId,superblock.freeStart,superblock.freeEnd,superblock.root,superblock.maxBlocks);
		fclose(sbFile);

		//Add content in freeblock list
		int j;		
		for (j=0;j<25;j++){
			int k;
			char fileName[100];
			sprintf(fileName,"%s%d",fusedata_path,j+1);
			FILE *fp = fopen(fileName, "w+");
			if(j==0){
				for(k=27;k<=399;k++ ){
					freeList[j][k-27]=k;
					fprintf(fp,"%d,",k);
				}
			}else{
				for(k=0;k<=399;k++ ){
					freeList[j][k]=(400 * (j))+k;
					fprintf(fp,"%d,",(400 * (j))+k);
				}
			}
			fclose(fp);

		}

		//Root Directory
		char fileName3[100];
		sprintf(fileName3,"%s%d",fusedata_path,26);		
		FILE *rootFile=fopen(fileName3,"w+");
		struct Directory dir;
		dir.size=0;
		dir.uid=1;
		dir.gid=1;
		dir.mode=16877;
		dir.atime=(long)time(NULL);
		dir.ctime=(long)time(NULL);
		dir.mtime=(long)time(NULL);
		dir.linkcount=2;
		strcpy(dir.fileToInodeDict,"d:.:26,d:..:26\0");
		fprintf(rootFile,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
				,dir.size,dir.uid,dir.gid,dir.mode,dir.atime,dir.ctime,dir.mtime,dir.linkcount,dir.fileToInodeDict);
		fclose(rootFile);
	}
}

int * getFreeBlock(){
	int i;
	int freeBlockList[25][400];
	int nextBlock;
	int freeBlockInfo[2];
	for(i=0;i<blockCount;i++){
		char fileName[100];
		int fileNum = i+1;
		sprintf(fileName,"%s%d",fusedata_path,fileNum);
		FILE *fp = fopen(fileName,"r+");
		if(!feof(fp)){
			fscanf(fp,"%d,",&nextBlock);			
			freeBlockInfo[0]=nextBlock;
			freeBlockInfo[1]= fileNum;
			fclose(fp);
			break;
		}
	}

	return freeBlockInfo;
}



void removeAssignedBlock(int* fileNum){
	char fileName[100] ;
	sprintf(fileName,"%s%d",fusedata_path,*fileNum);
	FILE *fp = fopen(fileName,"w+");
	if(!feof(fp)){
		int k;

		for(k=0;k<400;k++){
			if((k+1 < 400 &&(freeList[(*fileNum)-1][k+1]!=0)) ){

				freeList[(*fileNum)-1][k]=freeList[(*fileNum)-1][k+1];
				fprintf(fp,"%d,",freeList[(*fileNum)-1][k+1]);
			}
		}
	}
	fclose(fp);
}


void makeDirectory(char* dirName){

	int* nextBlockInfo = getFreeBlock();
	//Create inode for new directory and assign to free block

	char fileName[100];
	sprintf(fileName,"%s%d",fusedata_path,*(nextBlockInfo+0));
	FILE *fp = fopen(fileName,"w+");
	struct Directory dir;
	dir.size=0;
	dir.uid=1;
	dir.gid=1;
	dir.mode=16877;
	dir.atime=(long)time(NULL);
	dir.ctime=(long)time(NULL);
	dir.mtime=(long)time(NULL);
	dir.linkcount=2;
	char newFileInodeDict[1000];
	sprintf(dir.fileToInodeDict,"d:.:%d,d:..:%d\0",*(nextBlockInfo+0),currentDir);
	strcpy(newFileInodeDict,dir.fileToInodeDict);
	fprintf(fp,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,dir.size,dir.uid,dir.gid,dir.mode,dir.atime,dir.ctime,dir.mtime,dir.linkcount,dir.fileToInodeDict);
	fclose(fp);
	//update fileinodedict of  parent directory
	sprintf(fileName,"%s%d",fusedata_path,currentDir);
	FILE *fp1 = fopen(fileName,"r+w");

	struct Directory dir1;
	fscanf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,&dir1.size,&dir1.uid,&dir1.gid,&dir1.mode,&dir1.atime,&dir1.ctime,&dir1.mtime,&dir1.linkcount,dir1.fileToInodeDict);
	char insertInode[100];
	sprintf(insertInode,",d:%s:%d",dirName,*(nextBlockInfo+0));	
	char trimInodeDict[1000];
	strcpy(trimInodeDict,substring(dir1.fileToInodeDict,1,strlen(dir1.fileToInodeDict)-2));
	insert_substring(trimInodeDict,insertInode,strlen(trimInodeDict)+1);
	fseek(fp1,0,SEEK_SET);
	fprintf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,dir1.size,dir1.uid,dir1.gid,dir1.mode,dir1.atime,dir1.ctime,dir1.mtime,dir1.linkcount,trimInodeDict);
	fclose(fp1);
	removeAssignedBlock((nextBlockInfo+1));
}
struct Directory getDirInode(int id){

	char fileName[100];
	sprintf(fileName,"%s%d",fusedata_path,id);
	FILE *fp1 = fopen(fileName,"r+");

	struct Directory dir1;
	fscanf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,&dir1.size,&dir1.uid,&dir1.gid,&dir1.mode,&dir1.atime,&dir1.ctime,&dir1.mtime,&dir1.linkcount,dir1.fileToInodeDict);
	fclose(fp1);	
	char trimInodeDict[1000];
	strcpy(trimInodeDict,substring(dir1.fileToInodeDict,1,strlen(dir1.fileToInodeDict)-2));
	return dir1;
}
void CreateFile(char* name){
	int* inodeBlock = getFreeBlock();
	int inodeBlockValue = *(inodeBlock+0);	
	//Create inode for new file and assign to free block
	removeAssignedBlock((inodeBlock+1));
	int* dataBlock = getFreeBlock();
	char fileName[100];
	sprintf(fileName,"%s%d",fusedata_path,inodeBlockValue);
	FILE *fp = fopen(fileName,"w+");
	struct File file;
	file.size=0;
	file.uid=1;
	file.gid=1;
	file.mode=33267;
	file.atime=(long)time(NULL);
	file.ctime=(long)time(NULL);
	file.mtime=(long)time(NULL);
	file.linkcount=2;
	file.indirect=0;
	file.location=*(dataBlock+0);

	fprintf(fp,"{size:%d,uid:%d,gid:%d,mode:%d,linkcount:%d,atime:%ld,ctime:%ld,mtime:%ld,indirect:%d,location:%d}"
			,file.size,file.uid,file.gid,file.mode,file.linkcount,file.atime,file.ctime,file.mtime,file.indirect,file.location);
	fclose(fp);
	//update fileinodedict of  parent directory
	sprintf(fileName,"%s%d",fusedata_path,currentDir);
	FILE *fp1 = fopen(fileName,"r+w");

	struct Directory dir1;
	fscanf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,&dir1.size,&dir1.uid,&dir1.gid,&dir1.mode,&dir1.atime,&dir1.ctime,&dir1.mtime,&dir1.linkcount,dir1.fileToInodeDict);
	char insertInode[100];
	sprintf(insertInode,",f:%s:%d",name,inodeBlockValue);
	char trimInodeDict[1000];
	strcpy(trimInodeDict,substring(dir1.fileToInodeDict,1,strlen(dir1.fileToInodeDict)-2));
	insert_substring(trimInodeDict,insertInode,strlen(trimInodeDict)+1);
	fseek(fp1,0,SEEK_SET);
	fprintf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,fileToInodeDict:{%s}}"
			,dir1.size,dir1.uid,dir1.gid,dir1.mode,dir1.atime,dir1.ctime,dir1.mtime,dir1.linkcount,trimInodeDict);
	fclose(fp1);

	removeAssignedBlock((dataBlock+1));

}
char* SearchDirectory(char* path){
	char path1[100];
	const char s[2] = "/";
	strcpy(path1,path);
	char *token;

	/* get the first token */
	token = strtok(path1, s);
	struct Directory dir;
	dir = getDirInode(currentDir);
	char* dict;
	/* walk through other tokens */
	while( token != NULL )
	{

		dict=strstr(dir.fileToInodeDict,token);
		if(dict==NULL)
		{
			return NULL;
		}
		else
		{
			token = strtok(NULL, s);
		}
	}
	char* inode;
	inode =  strtok(dict,",");
	char* inodeNum;
	inodeNum=strtok(inode,":");
	inodeNum = strtok(NULL,inode);
	return inodeNum;

}
struct File GetFile(int id){

	char fileName[100];
	sprintf(fileName,"%s%d",fusedata_path,id);
	FILE *fp1 = fopen(fileName,"r+");

	struct File file;
	fscanf(fp1,"{size:%d,uid:%d,gid:%d,mode:%d,linkcount:%d,atime:%ld,ctime:%ld,mtime:%ld,indirect:%d,location:%d}"
			,&file.size,&file.uid,&file.gid,&file.mode,&file.linkcount,&file.atime,&file.ctime,&file.mtime,&file.indirect,&file.location);
	fclose(fp1);

	return file;
}
void* my_init(struct fuse_conn_info *conn)
{
	void *ptr;    // ptr is declared as Void pointer
	char cnum;
	ptr = &cnum;  // ptr has address of character data
	initialSetup();	
	return ptr;
}

static int my_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {

		struct Directory dir;
		/* walk through other tokens */

		dir = getDirInode(currentDir);

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		if(dir.size!=0){
			stbuf->st_size=dir.size;
		}
	} else {
		char path1[100];
		const char s[2] = "/";
		strcpy(path1,path);
		char *token;

		/* get the first token */
		token = strtok(path1, s);
		struct Directory dir;

		/* walk through other tokens */
		while( token != NULL )
		{

			dir = getDirInode(26);
			char* dict;
			dict=strstr(dir.fileToInodeDict,token);
			if(dict==NULL)
			{
				return -ENOENT;
			}
			else{
				char tempToken[100]="f:";
				strcat(tempToken,token);
				char* type;
				type = strstr(dir.fileToInodeDict,tempToken);
				if(type==NULL){
					stbuf->st_mode = 16877;//S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_atime=time(NULL);
					stbuf->st_ctime=time(NULL);
					stbuf->st_mtime=time(NULL);
					if(dir.size!=0){
						stbuf->st_size=dir.size;
					}
				}else{
					stbuf->st_mode = 33267;//S_ISFIFO//S_IFREG | 0644;
					stbuf->st_nlink = 1;
					stbuf->st_atime=time(NULL);
					stbuf->st_ctime=time(NULL);
					stbuf->st_mtime=time(NULL);
					if(dir.size!=0){
						stbuf->st_size=dir.size;
					}
				}
				token = strtok(NULL, s);
			}
		}


	}

	return res;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi)
{

	(void) offset;
	(void) fi;
	struct Directory dir;
	dir = getDirInode(26);
	char* inode ;
	inode = strtok(dir.fileToInodeDict,":");
	int i=0;
	/* walk through other tokens */
	while( inode != NULL )
	{
		if(i%2 !=0)
			filler(buf,inode,NULL,0);
		inode=strtok(NULL,":");
		i++;

	}

	return 0;


}
static int my_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	char path1[100];
	const char s[2] = "/";
	strcpy(path1,path);	
	char *token;

	/* get the first token */
	token = strtok(path1, s);
	CreateFile(token);
	return 0;

}
static int my_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	int res=0;
	int inodeId = atoi(SearchDirectory(path));
	struct File file = GetFile(inodeId);
	if(size<=4096){
		char fileName[100];
		sprintf(fileName,"%s%d",fusedata_path,file.location);
		FILE *fp = fopen(fileName,"w+");
		res=fprintf(fp,"%s",buf);
		fclose(fp);
	}
	return res;
}
static int my_utimens(const char *path, const struct timespec ts[2])
{
	return 0;
}


static int my_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int res=0;
	int inodeId = atoi(SearchDirectory(path));	
	struct File file = GetFile(inodeId);
	if (offset >= 4096)
		return 0;
	if (size > 4096 - offset)
		size = 4096 - offset;

	if(size<=4096){
		char fileName[100];
		sprintf(fileName,"%s%d",fusedata_path,file.location);
		FILE *fp = fopen(fileName,"r+");
		res=fscanf(fp,"%s",buf);
		fclose(fp);
	}
	return res;

}
static int my_mkdir(const char *path, mode_t mode){

	char path1[100];
	const char s[2] = "/";
	strcpy(path1,path);
	char *token;

	/* get the first token */
	token = strtok(path1, s);
	makeDirectory(token);

	return 0;
}



static struct fuse_operations my_oper = {
		.getattr	= my_getattr,
		.readdir	= my_readdir,
		.open		= my_open,
		.read		= my_read,
		.init 		= my_init,
		.mkdir      = my_mkdir,
		.create		= my_create,
		.utimens	= my_utimens,
		.write		= my_write,

};

int main(int argc, char *argv[])
{
	setFuseDataPath();
	return fuse_main(argc, argv, &my_oper, NULL);
}

