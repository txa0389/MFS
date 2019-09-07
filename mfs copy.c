
/*

	Compling
	g++ mfs.c -o mfs
*/

#include <stdio.h> //*
#include <string.h> //*
#include <stdlib.h> //*
#include <ctype.h> //*
#include <stdint.h> //*
#include <unistd.h> //
#include <sys/wait.h> //
#include <errno.h> //
#include <signal.h> //


#define READ_ONLY  0x01
#define HIDDEN     0x02
#define DIRECTORY  0x10
#define ARCHIVE    0x20


struct __attribute__((__packed__)) DirectoryEntry 
{
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t Unused1[8];
	uint16_t DIR_FirstClusterHigh;
	uint8_t Unused2[4];
	uint16_t DIR_FirstClusterLow;
	uint32_t DIR_FileSize;
};


struct FAT32 
{
	char BS_OEMNAME[8];
	uint16_t BPB_BytsPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATS;
	uint16_t BPB_RootEntCnt;
	uint32_t BPB_FATSz32;
	uint32_t BPB_RootClusAddresults;
	char BS_VolLab[11];
	int RootDirSectors;
	int FirstDataSector;
	int first_clu;
	int root_offset;
	int bytesPerCluster;
};

// functions
int LBAToOffset(int32_t sector, struct FAT32 *fat);
int16_t nextLB(uint32_t sector, FILE *fp, struct FAT32 *fat);
char* formatFileString(char* userInput);
int checkFile(struct DirectoryEntry *dir, char* filename);
void get_all_data(FILE *file, struct FAT32 *img);
FILE* openFile(char *fileName, struct FAT32 *img, struct DirectoryEntry *dir);
void ls(FILE *file, struct FAT32 *fat, struct DirectoryEntry *dir);
void stat(struct DirectoryEntry *dir, FILE *file, char* userFileName);
void get(FILE *file, struct DirectoryEntry *dir, struct FAT32 *fat, char* userCleanName, char* userOriginalName);
void readFile(FILE *file, struct FAT32 *fat, struct DirectoryEntry dir, int offset, int numOfBytes);
void readDirectory(int cluster, FILE *file, struct DirectoryEntry *dir, struct FAT32 *fat);



int main()
{
	int currentOffset = 0;
	char * complete_str = (char*) malloc( 255 );
	char * currentFile = NULL;
	int check = 0;
	FILE *IMAGE = NULL;

	struct FAT32 *fat = (struct FAT32 *)malloc(sizeof(struct FAT32));
	struct DirectoryEntry *dir = (struct DirectoryEntry *)malloc(sizeof(struct DirectoryEntry) * 16);
	
	while(1)
	{
		printf ("mfs> ");
		while( !fgets (complete_str, 255, stdin) );
		char *token[5];
		int token_count = 0;
		char *tk_counter;  
		char *working_str  = strdup( complete_str );                
		char *working_root = working_str;

		//seperating the string
		char *part1=strsep(&working_str," \t\n");

		while (part1 != NULL && (token_count<5))
		{
			token[token_count] = strndup( tk_counter, 255 );
			if( strlen( token[token_count] ) == 0 )
			{
				token[token_count] = NULL;
			}
				token_count++;
		}


		if ( token[0] == NULL)
		{
			//Back to loop
		}
		
		else if(strcmp(token[0], "open") == 0)
		{
			if(currentFile == NULL && IMAGE == NULL)
			{
				if(token[1] != NULL)
				{
					IMAGE = openFile(token[1], fat, dir);
					if(IMAGE != NULL)
					{
						check = 0;
						currentFile = (char *)malloc(sizeof(token[1]));
						strcpy(currentFile, token[1]);
					}
				}
			}
			else
			{
				if(strcmp(currentFile, token[1]) == 0)
				{
					printf("Error: File system image is already open.\n");
				}
			}
		}
		// CLose Function : Close fat32.img
		else if ( strcmp(token[0], "close") == 0){
			
			if(check != 1){
				
				if(IMAGE != NULL){

					int result = fclose(IMAGE);
					if(result == 0)
					{
						currentFile = NULL;
						IMAGE = NULL;
						check = 1;
					}
				}
				else
				{
					printf("Error: File system not open.");
				}
			}
			else
			{
				printf("File system Image not open");
			}
		}
		else if (strcmp(token[0], "stat") == 0)
		{
			if(IMAGE != NULL)
			{
					if(token[1] != 0)
					{
						char *nwptr = NULL;
						nwptr = formatFileString(token[1]);
						stat(dir, IMAGE, nwptr);
					}
			}
		}
		else if (strcmp(token[0], "get") == 0)
		{
		 if(IMAGE != NULL)
		 {
				if(token[1] != NULL)
				{
					char *nwptr = NULL;
					nwptr = formatFileString(token[1]);
					get(IMAGE, dir, fat, nwptr, token[1]);
				}
			}
		}
		else if (strcmp(token[0], "cd") == 0) {
			char * nwptrFileName = NULL;
			int fIndex;
			char * delimeter = (char *)"/";
			char buffer[strlen(token[1])];
			char * fileToken;
			char * fileTokens[50];
			
			strcpy(buffer, token[1]);
			
			int i = 0;
			int tCounter = 0;
			
			// tokenize the string after "cd" was entered
			fileToken = strtok ( buffer, delimeter);
			while (fileToken != NULL) {
				fileTokens[tCounter] = (char *)malloc(sizeof(strlen(fileToken)));
				strcpy(fileTokens[tCounter], fileToken);
				fileToken = strtok(NULL, delimeter);
				tCounter++;
			}


			if (IMAGE != NULL) {
				// For each directory taken out of token[1]
				// cd into the directory
				for ( i = 0; i < tCounter; i++ ) {
					// change the user string a fat32 recognizable string 
					nwptrFileName = formatFileString(fileTokens[i]);
					// get the file index if the files exsists
					if ((fIndex = checkFile(dir, nwptrFileName)) != -1) {
						// if the folder is a valid directory (DIRECTORY) read the directory
						if (dir[fIndex].DIR_Attr == DIRECTORY) { 
							readDirectory(dir[fIndex].DIR_FirstClusterLow, IMAGE, dir, fat);
							currentOffset = dir[fIndex].DIR_FirstClusterLow;
							
						} else if (dir[fIndex].DIR_Name[0] == '.') {
							readDirectory(dir[fIndex].DIR_FirstClusterLow, IMAGE, dir, fat);
							currentOffset = dir[fIndex].DIR_FirstClusterLow;
							
						}
					} 
				}
			}
		} 
		else if (strcmp(token[0], "ls") == 0) {
			if(IMAGE != NULL)
			{ 
				int fIndex;
				int leaves = 0;
				char * nwptrFileName = NULL;
				char * fileToken;
				char * fileTokens[50];
				char * delimeter = (char *)"/";
				int i = 0;
				int tCounter = 0;
				int  count_dot= 0;
			
			if (token[1] != NULL ) {
				char buffer[strlen(token[1])];
				strcpy(buffer, token[1]);
				fileToken = strtok ( buffer, delimeter);

				while (fileToken != NULL) {
					
					fileTokens[tCounter] = (char *)malloc(sizeof(strlen(fileToken)));
					strcpy(fileTokens[tCounter], fileToken);
					
					fileToken = strtok(NULL, delimeter);

					if (strcmp(fileTokens[tCounter], "..") == 0) {
						leaves--;
					} else if  (strcmp(fileTokens[tCounter], ".") == 0) {
						count_dot++;
					}
					else {
						leaves++;
					} 
					
					tCounter++;
				}
			} 
				if ( tCounter != 0 ) {
					for ( i = 0; i < tCounter; i++ ) {
					nwptrFileName = formatFileString(fileTokens[i]);
					
						if ((fIndex = checkFile(dir, nwptrFileName)) != -1) {
							if (dir[fIndex].DIR_Attr == DIRECTORY) {
								readDirectory(dir[fIndex].DIR_FirstClusterLow, IMAGE, dir, fat);
							} else if (dir[fIndex].DIR_Name[0] == '.') {
								readDirectory(dir[fIndex].DIR_FirstClusterLow, IMAGE, dir, fat);
							}
						} 
					}
						ls(IMAGE, fat, dir);
						for (i = 0; i < leaves; i++)
						{
							readDirectory(dir[1].DIR_FirstClusterLow, IMAGE, dir, fat);
						}
					}
					else 
					{
						ls(IMAGE, fat, dir);
					}
			}
		}
		else if (strcmp(token[0], "read") == 0) {
			int offset;
			int numOfBytes;
			char *nwptrFileName = NULL;
			if(token[1] != NULL || token[2] != NULL || token[3] != NULL){
				nwptrFileName = formatFileString(token[1]);
				offset = atoi(token[2]);
				numOfBytes = atoi(token[3]);
				int fIndex;
				if(IMAGE != NULL){
					if((fIndex = checkFile(dir, nwptrFileName)) != -1){
						readFile(IMAGE, fat, dir[fIndex], offset, numOfBytes);
					}
				}
			}
		}
		else if (strcmp(token[0], "volume") == 0) {
			if(IMAGE != NULL){
				char * vol;
				vol = (char *)malloc(sizeof(11));
				
				strcpy(vol, fat->BS_VolLab);
				if( vol != NULL){
					printf("Volume is :%s\n", vol);
				}else{
					printf("%s\n", "Error: volume name not found.");
				}
			}
		}

		free( working_root );
	}
	return 0;
}

FILE* openFile(char *fileName, struct FAT32 *img, struct DirectoryEntry *dir)
{
	FILE *file;
	if(!(file=fopen(fileName, "rb")))
	{
		printf("Error: File system image not found.\n");
		return 0;
	}

	get_all_data(file,img);

	img->RootDirSectors = 0;
	img->FirstDataSector = 0;
	img->first_clu =  0;

	img->root_offset = (img->BPB_NumFATS * img->BPB_FATSz32 * img->BPB_BytsPerSec) + (img->BPB_RsvdSecCnt * img->BPB_BytsPerSec);
	img->bytesPerCluster = (img->BPB_SecPerClus * img->BPB_BytsPerSec);


	fseek(file, img->root_offset, SEEK_SET);
	int i = 0;

	for(i=0; i<16; i++){
		fread(&dir[i], 32, 1, file);
	}

	// return a file pointer
	return file;
}

void readFile(FILE *file, struct FAT32 *fat, struct DirectoryEntry dir, int offset, int numOfBytes){
	uint8_t value;
	int userOffset = offset;
	int cluster = dir.DIR_FirstClusterLow;
	int fileOffset = LBAToOffset(cluster, fat);
	fseek(file, fileOffset, SEEK_SET);

	// fread(&value, numOfBytes, 1, file);
	// printf("%d", value);
 
	while(userOffset > fat->BPB_BytsPerSec){
		 cluster = nextLB(cluster, file, fat);
		 userOffset -= fat->BPB_BytsPerSec;
	}

	fileOffset = LBAToOffset(cluster, fat);
	fseek(file, fileOffset + userOffset, SEEK_SET);
	int i = 0;
	for(i = 0; i < numOfBytes; i++){
		fread(&value, 1, 1, file);
		printf("%d", value);
	}
	printf("\n");
}

void ls(FILE *file, struct FAT32 *fat, struct DirectoryEntry *dir){
	// files with the archive flag
	int i = 0;
	signed char firstByteOfDIRName=  dir[2].DIR_Name[0];
	// Looks at all 16 directories
	for(i=0; i < 16; i++){
		
		// Looks at first character is directory and comparesult it to 0xe5 in hex
		signed char firstByteOfDIRName=  dir[i].DIR_Name[0];
		if ( firstByteOfDIRName == (char)0xe5 ) {
			int j = 1;
		} 
		else if (dir[i].DIR_Attr == DIRECTORY || dir[i].DIR_Attr == ARCHIVE || dir[i].DIR_Attr == READ_ONLY ||  dir[i].DIR_Name[0] == '.')  {
			// temp char array for name
			char fileName[12];
			memset(fileName, 0, 12);
			strncpy(fileName, dir[i].DIR_Name, 11);
			printf("%2s\n", fileName);
		} 
	}
}


void stat(struct DirectoryEntry *dir, FILE *file, char* userFileName){
	int fIndex;
	if((fIndex = checkFile(dir, userFileName)) != -1)
	{
		printf("File Size: %d\n", dir[fIndex].DIR_FileSize);
		printf("First Cluster Low: %d\n",  dir[fIndex].DIR_FirstClusterLow);
		printf("DIR_ATTR: %d\n", dir[fIndex].DIR_Attr);
		printf("First Cluster High: %d\n", dir[fIndex].DIR_FirstClusterHigh);
	
	} 
	else 
	{
		printf("%s\n", "File not found.");
	}
}


void get(FILE *file, struct DirectoryEntry *dir, struct FAT32 *fat, char* userCleanName, char* userOriginalName){
	int fIndex;
	if((fIndex = checkFile(dir, userCleanName)) != -1){
		// if it does
		FILE *localFile;
		int nextCluster;
		localFile = fopen(userOriginalName, "w");
		int size = dir[fIndex].DIR_FileSize;
		int cluster = dir[fIndex].DIR_FirstClusterLow;
		int offset = LBAToOffset(cluster, fat);

		fseek(file, offset, SEEK_SET);
		nextCluster = cluster;
		uint8_t value[512];
		while(size > 512){
			fread(&value, 512, 1, file);
			fwrite(&value, 512, 1, localFile);
			size -= 512;
			nextCluster = nextLB(nextCluster, file, fat);
			fseek(file, LBAToOffset(nextCluster, fat), SEEK_SET);
		}
		fread(&value, size, 1, file);
		fwrite(&value, size, 1, localFile);
		fclose(localFile);
		
	}else{
		printf("%s\n", "File not found.");
	}
}


int LBAToOffset(int32_t sector, struct FAT32 *fat)
{
	return ((sector - 2) * fat->BPB_BytsPerSec) + (fat->BPB_BytsPerSec * fat->BPB_RsvdSecCnt) + (fat->BPB_NumFATS * fat->BPB_FATSz32 * fat->BPB_BytsPerSec);
}

int16_t nextLB(uint32_t sector, FILE *fp, struct FAT32 *fat)
{
	int FATAddresults = (fat->BPB_BytsPerSec * fat->BPB_RsvdSecCnt) + (sector * 4);
	int16_t val;
	fseek(fp, FATAddresults, SEEK_SET);
	fread(&val, 2, 1, fp);
	return val;
}

char* formatFileString(char* userInput) 
{
	char dup_data[strlen(userInput)];
	strcpy(dup_data, userInput);

	char *filename, *extension, *token, *char_fat_string, *delimeter;
	char_fat_string = (char*)malloc(sizeof(char) * 11);
	delimeter = (char *) ".\n";

	int numOfSpaces;
	int numOfExtSpaces = 3;

	if ( dup_data[0] == '.' && dup_data[1] == '.'){
		char_fat_string = (char *) "..         ";
		 
	} else if ( dup_data[0] == '.' ) {
		char_fat_string = (char *) ".          ";

	} else {
		

		token = strtok(dup_data,delimeter);
		filename = (char *)malloc(sizeof(token));
		strcpy(filename, token);


		int lenOfExtension;
		if((token = strtok(NULL, delimeter)) != NULL) {
			extension = (char *)malloc(sizeof(token));
			strcpy(extension, token);
			lenOfExtension = strlen(extension);
			numOfExtSpaces = 3 - lenOfExtension;
		} else {

			extension = (char *)malloc(sizeof(0));
			extension = (char *) "";
			numOfExtSpaces = 3;
		}
		
		int lenOfFilename = strlen(filename);
		numOfSpaces = 8 - lenOfFilename;
		strcat(char_fat_string, filename);
		if(numOfSpaces > 0){
			int i = 0;
			for(i = 0; i < numOfSpaces; i++){
				strcat(char_fat_string, " ");
			}
		}

		strcat(char_fat_string, extension);
	
		if(numOfExtSpaces > 0){
			int i = 0;
			for(i = 0; i < numOfExtSpaces; i++){
				strcat(char_fat_string, " ");
			}
		}
		int i = 0;
		for(i = 0; i < strlen(char_fat_string); i++){
			char_fat_string[i] = toupper(char_fat_string[i]);
		}

		return char_fat_string;
	}
	return char_fat_string;
}

int checkFile(struct DirectoryEntry *dir, char* filename){
	int i = 0;
	for(i=0; i < 16; i++){
		if(dir[i].DIR_Attr == DIRECTORY || dir[i].DIR_Attr == ARCHIVE || dir[i].DIR_Name[0] == '.'){
			char dirFileName[12];
			memset(dirFileName, 0, 12);
			strncpy(dirFileName, dir[i].DIR_Name, 11);
			if(strcasecmp(dirFileName, filename) == 0){
				return i;
			}
		}
	}
	return -1;
}

void get_all_data(FILE *file, struct FAT32 *img)
	{

	fseek(file, 11, SEEK_SET);
	fread(&img->BPB_BytsPerSec, 2, 1, file);

	fseek(file, 13, SEEK_SET);
	fread(&img->BPB_SecPerClus, 1, 1, file);

	fseek(file, 14, SEEK_SET);
	fread(&img->BPB_RsvdSecCnt, 2, 1, file);

	fseek(file, 16, SEEK_SET);
	fread(&img->BPB_NumFATS, 1, 1, file);
 
	fseek(file, 36, SEEK_SET);
	fread(&img->BPB_FATSz32, 4, 1, file);

	fseek(file, 3, SEEK_SET);
	fread(&img->BS_OEMNAME, 8, 1, file);

	fseek(file, 71, SEEK_SET);
	fread(img->BS_VolLab, 11, 1, file);
	
	fseek(file, 17, SEEK_SET);
	fread(&img->BPB_RootEntCnt, 2, 1, file);

	fseek(file, 44, SEEK_SET);
	fread(&img->BPB_RootEntCnt, 2, 1, file);

	}

void readDirectory(int cluster, FILE *file, struct DirectoryEntry *dir, struct FAT32 *fat) {
	int offset;
	if (cluster == 0) {
		offset = fat->root_offset;
	} else {
		offset = LBAToOffset(cluster, fat);
	}
	fseek(file, offset, SEEK_SET);
	int i;
	for(i=0; i<16; i++){
		fread(&dir[i], 32, 1, file);
	 
	}
}
