#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

typedef struct
{
  int  magic;             /* This must have the value  0x63746172.                        */
  int  eop;               /* End of file pointer.                                         */
  int  block_count;       /* Number of entries in the block which are in-use.             */
  int  file_size[4];      /* File size in bytes for files 1..4                            */
  char deleted[4];        /* Contains binary one at position i if i-th entry was deleted. */
  int  file_name[4];      /* pointer to the name of the file.                             */
  int  next;              /* pointer to the next header block.                            */
} hdr;

int createEmptyArchive(char* filename);
int appendArchive(char* filename, int argc, char** argv);
int validateArchive(char* filename);
int calculateEOP(int argc, char** argv);
int deleteArchive(char* filename, char* dfile);

int deleteArchive(char* filename, char* dfile)
{
	int i, fd, bytesWritten, bytesRead, stringLength, stringCounter;
	int ci = 0;
	char c;
	
    if(access(filename,F_OK) != -1)
    {
        //file already exists
        if(validateArchive(filename) != -1)
        {
			fd = open(filename,O_RDWR);
			if(fd == -1)
			{
				write(1,"open() has failed and returned an error...\n",43);
				return -1;
			}
			
			hdr header;
			bytesRead = read(fd,&header,sizeof(hdr));
			if(bytesRead == -1) return -1;
			//find the header for the given file
			while(bytesRead != 0)
			{
                //loop through each file name within the block
				for(i = 0; i < header.block_count; i++)
				{
                    //grab the file name itself
					lseek(fd, header.file_name[i], SEEK_SET);
					stringLength = 0;
					stringCounter = 0;
					while(read(fd, &c, 1) != 0) 
					{
						stringLength++;
						if(c == '\0') break;
					}
					lseek(fd, (-1 * stringLength),SEEK_CUR);
					char nameOfFile[stringLength];
					while(stringCounter < stringLength)
					{
						read(fd, &c, 1);
						nameOfFile[stringCounter] = c;
						stringCounter++;
					}

                    //check if the name of the file is equal to the file argument
					if(strcmp(dfile,nameOfFile) == 0)
					{
                        //set the delete flag and rewrite the header
						header.deleted[i] = 1;
						lseek(fd, ci, SEEK_SET);
						bytesWritten = write(fd, &header, sizeof(hdr));
						if(bytesWritten == -1) return -1;
						close(fd);
						return 0;
					}
				}
				
                //increment to the next header
				lseek(fd, header.next, SEEK_SET);
                ci = header.next;
				bytesRead = read(fd,&header,sizeof(hdr));
				if(bytesRead == -1) return -1;
			}
			return -1;
		}
		else
        {
            //file failed to validate
            write(1, "The given file is not a valid archive\n", 38);
            return -1;
        }
	}
	else
	{
		//file failed to validate
		write(1, "The specified archive file does not exist\n", 42);
		return -1;
	}
}

int createEmptyArchive(char* filename)
{
    if(access(filename,F_OK) != -1)
    {
        //file exists
        write(1,"A file or archive file with that name already exists\n",53);
        return -1;
    }
    else
    {
        //file doesn't exist
        int fd, bytesWritten;        

        hdr emptyArchive = {.magic = 0x63746172, .eop = sizeof(hdr), .block_count = 0, .file_size = {0}, .deleted = {0},
            .file_name = {0}, .next = sizeof(hdr)};
        
        //create the file with the given archive name and open it
        fd = open(filename,O_RDWR|O_CREAT,0644);   
        if(fd == -1)
        {
            write(1,"open() has failed and returned an error...\n",43);
            return -1;
        }
        else
        {
            //write the empty header and close the file
            bytesWritten = write(fd,&emptyArchive,sizeof(hdr));
            close(fd);
            return bytesWritten;
        }
    }
}

int appendArchive(char* filename, int argc, char** argv)
{
    int i, j, fd, bytesWritten, bytesRead;
    int fullHeader = 0;
    int ci = sizeof(hdr);
    if(access(filename,F_OK) != -1)
    {
        //file already exists
        if(validateArchive(filename) != -1)
        {
            //file is validated
			int startingOffset = 0;
			int lastHeader = 0;
			
			fd = open(filename,O_RDWR);
			if(fd == -1)
			{
				write(1,"open() has failed and returned an error...\n",43);
				return -1;
			}
			else
			{
				hdr header;
				bytesRead = read(fd,&header,sizeof(hdr));
				if(bytesRead == -1) return -1;
                //loop through to find the last header within the archive
				while(!lastHeader)
				{
                    //if the header is not full we found it
					if(header.block_count < 4)
					{
						lastHeader = 1;
                        startingOffset = header.block_count;
                        ci = header.next;
					}
					else
					{
                        //else we increment the currency indicator and read the next header
						lseek(fd, header.next, SEEK_SET);
						bytesRead = read(fd,&header,sizeof(hdr));
						if(bytesRead == -1) return -1;
					}
				}
				
				//add new entries
				struct stat fileStat;
                
                //loop through all the files given via command line
				for(i = 3; i < argc; i++)
				{
					if(fstat(open(argv[i],O_RDONLY),&fileStat) < 0)
					{
						write(1,"fstat has failed\n",17);
						return -1;
					}
					else
					{
                        //update all the header information
						header.file_size[((i+1+startingOffset)%4)] = (int) fileStat.st_size;
						header.block_count++;
						header.file_name[((i+1+startingOffset)%4)] = ci;
						ci += (strlen(argv[i])+1) + fileStat.st_size;
						header.next = ci;
					}
                    //if current position represents 4 files being read into the header we need to write the header and file data
					if(((i+2+startingOffset) % 4 == 0 && i != 3) || i == (argc-1))
					{
                        //if it is the last header we need to update its information to include the new files
						if(lastHeader == 1)
						{
							lseek(fd, (-1 * sizeof(hdr)), SEEK_CUR);	
							lastHeader = 0;
						}
                        //if the last header is full and we have nothing else to write then set the fullHeader flag
                        if(header.block_count == 4 && i == (argc-1))
                        {
                            fullHeader = 1;
                        }
                        //write the header
						bytesWritten = write(fd, &header, sizeof(hdr));
						if(bytesWritten == -1) return -1;
						//reset needed header values
						memset(header.file_size, 0, sizeof(header.file_size));
						memset(header.deleted, 0, sizeof(header.deleted));
						memset(header.file_name, 0, sizeof(header.file_name));
						header.block_count = 0;
                        //might need to remove this
                        ci += sizeof(hdr);
                        //reset our currency indicator to the end of the file
                        lseek(fd,0,SEEK_END);
						//now write the 4 file names and headers
						for(j = ((i+1)%4); j >= 0; j--)
						{
							//need to add 1 to the string length because it does not include the null terminator
                            bytesWritten = write(fd, argv[i-j], (strlen(argv[i-j]) + 1));
                            if(bytesWritten == -1) return -1;
                            int fd2;
							char c;
							fd2 = open(argv[i-j], O_RDWR);
                            //write all the data to the file
							while(read(fd2, &c, 1) != 0)
							{
								bytesWritten = write(fd,&c,1);
								if(bytesWritten == -1) return -1;
							}
							close(fd2);
						}
                        //if the last header is full and we have no more to write then write a blank header
                        if(fullHeader == 1)
                        {
                            header.next += sizeof(hdr);
                            bytesWritten = write(fd, &header, sizeof(hdr));
                            if(bytesWritten == -1) return -1;
                        }
					}
				}
                //update eops for all headers in the archive
                int newEnd = (int) lseek(fd, 0, SEEK_END);
                lseek(fd, 0, SEEK_SET);
                bytesRead = read(fd,&header,sizeof(hdr));
                if(bytesRead == -1) return -1;
                do
                {
                    header.eop = newEnd;
                    lseek(fd, (-1 * sizeof(hdr)), SEEK_CUR);
                    bytesWritten = write(fd, &header, sizeof(hdr));
                    lseek(fd, header.next, SEEK_SET);
                    bytesRead = read(fd, &header, sizeof(hdr));
                    if(bytesRead == -1) return -1;
                }
                while(header.next != newEnd);
				close(fd);
			}
            return 0;
        }
        else
        {
            //file failed to validate
            write(1, "The given file is not a valid archive\n", 38);
            return -1;
        }
    }
    else
    {
        //archive file does not exist so we create and open it
        fd = open(filename,O_RDWR|O_CREAT,0644);
        if(fd == -1)
        {
            write(1,"open() has failed and returned an error...\n",43);
            return -1;
        }
        else
        {
            struct stat fileStat;
            hdr header = {.magic = 0x63746172, .eop = calculateEOP(argc, argv), .block_count = 0, .file_size = {0}, .deleted = {0}, .file_name = {0}, .next = 0};

            //loop through all the file names that were given
            for(i = 3; i < argc; i++)
            {
                //get the stats of the file
                if(fstat(open(argv[i],O_RDONLY),&fileStat) < 0)
                {
                    write(1,"fstat has failed\n",17);
                    return -1;
                }
                else
                {
                    //update all header information for each file
                    header.file_size[((i+1)%4)] = (int) fileStat.st_size;
                    header.block_count++;
                    header.file_name[((i+1)%4)] = ci;
                    ci += (strlen(argv[i])+1) + fileStat.st_size;
                    header.next = ci;
                }
                //if current position represents 4 files being read into the header we need to write the header and file data
                if(((i+2) % 4 == 0 && i != 3) || i == (argc-1))
                {
                    //write the header
                    bytesWritten = write(fd, &header, sizeof(hdr));
                    if(bytesWritten == -1) return -1;
                    //reset needed header values
                    memset(header.file_size, 0, sizeof(header.file_size));
                    memset(header.deleted, 0, sizeof(header.deleted));
                    memset(header.file_name, 0, sizeof(header.file_name));
                    header.block_count = 0;
                    ci += sizeof(hdr);

                    //now write the file names and headers
                    for(j = ((i+1)%4); j >= 0; j--)
                    {
                        //manually add one for the null terminator when writing program names
                        bytesWritten = write(fd, argv[i-j], (strlen(argv[i-j]) + 1));
                        if(bytesWritten == -1) return -1;
                        int fd2;
                        char c;
                        fd2 = open(argv[i-j], O_RDWR, 0644);
                        while(read(fd2, &c, 1) != 0)
                        {
                            bytesWritten = write(fd,&c,1);
                            if(bytesWritten == -1) return -1;
                        }
                        close(fd2);
                    }
                }
            }
            close(fd);
            return 0;
        }
    }
}

int validateArchive(char* filename)
{
    int fd, bytesRead;
    fd = open(filename,O_RDWR);
    if(fd == -1)
    {
        write(1,"open() has failed and returned an error...\n",43);
        return -1;
    }
    else
    {
        //read the first header in the file
        hdr header;
        bytesRead = read(fd,&header,sizeof(hdr));
        if(bytesRead == -1)
        {
            write(1,"read() has failed and returned an error...\n",43);
            return -1;
        }
        else
        {
            if(header.magic == 0x63746172 && header.eop == lseek(fd,0,SEEK_END))
            {
                //we have validated it is an archive file
                close(fd);
                return bytesRead;
            }
            else
            {
                //the file exists but it is not an archive file
                close(fd);
                return -1;
            }
        }
    }
}

//method used to calculate the final EOP given files from arguments
int calculateEOP(int argc, char** argv)
{
    int i, fd;
    int totalBytes = 0;
    
    for(i = 3; i < argc; i++)
    {
        fd = open(argv[i],O_RDWR);
        if(fd == -1)
        {
            write(1,"open() has failed and returned an error...\n",43);
            return -1;
        }
        else
        {
            struct stat fileStat;
            if(fstat(fd,&fileStat) < 0)
            {
                write(1,"fstat has failed\n",17);
                return -1;
            }
            else
            {
                //add the file name and file size to the total bytes
                totalBytes += (strlen(argv[i]) + 1);
                totalBytes += fileStat.st_size;
            }
            //if is a multiple of 4 then add the size of a header
            if((i+1) % 4 == 0) totalBytes += sizeof(hdr);
        }
    }
    return totalBytes;
}

int main(int argc, char **argv)
{
    if(argc < 3)
    {
        //not enough arguments given
        write(1,"usage: ctar <-a/-d> <archive-file-name> <files...>\n",51);
        return 0;
    }
    else if(argc == 3)
    {
        //if no files are given. create an empty archive file
        if(strcmp(argv[1],"-a") == 0)
        {
            if(createEmptyArchive(argv[2]) != -1)
            {
                //the archive file was created successfully
                write(1,"Archive was successfully created.\n",34);
                return 0;
            }
            else
            {
                //there was an error creating the archive file
                write(1,"Failed to create empty archive\n",31);
                return 1;
            }
        }
        else
        {
            write(1,"usage: ctar <-a/-d> <archive-file-name> <files...>\n",51);
            return 0;
        }
    }
    else
    {
        //at least one input file name was given
        if(strcmp(argv[1],"-a") == 0)
        {
            if(appendArchive(argv[2], argc, argv) != -1)
            {
                write(1,"Successfully appended file(s) to archive\n",41);
                return 0;
            }
            else
            {
                write(1,"Failed to append file(s) to archive\n",36);
                return 1;
            }
        }
        //user can only delete one file at a time
        else if(strcmp(argv[1],"-d") == 0 && argc == 4)
        {
			if(deleteArchive(argv[2], argv[3]) != -1)
			{
				write(1,"File successfully deleted from archive\n",39);
                return 0;
			}
			else
			{
				write(1,"Failed to delete file from archive\n",35);
                return -1;
			}
        }
        else
        {
            write(1,"usage: ctar <-a/-d> <archive-file-name> <files...>\n",51);
            return 0;
        }
    }
}
