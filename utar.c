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

int validateArchive(char* filename);
int extractArchive(char* filename);

int extractArchive(char* filename)
{
		if(validateArchive(filename) != -1)
        {
            //file is validated
			int fd, fd2, bytesWritten, bytesRead, i, stringLength;
			int stringCounter = 0;
			int fileCounter = 0;
			char c;
			
			fd = open(filename,O_RDWR);
			if(fd == -1)
			{
				write(1,"open() has failed and returned an error...\n",43);
				return -1;
			}
			else
			{
                //read in the first header
				hdr header;
				bytesRead = read(fd,&header,sizeof(hdr));
				if(bytesRead == -1) return -1;
				while(bytesRead != 0)
				{
                    //loop through all the header blocks
					for(i = 0; i < header.block_count; i++)
					{						
                        //if the file has the delete flag then dont extract it
						if(header.deleted[i] == 1 && i < header.block_count)
                        {
                            lseek(fd,header.file_name[i+1], SEEK_SET);
                            continue;
                        }
                        //if the block is the last block go to the next header
                        else if(header.deleted[i] == 1 && i == header.block_count)
                        {
                            //if its the last header then simply break
                            if(header.next == header.eop)
                            {
                                break;
                            }
                            else
                            {
                                lseek(fd,header.next,SEEK_SET);
                                break;
                            }
                        }

						//read the name of the file
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
                        //check if the file exists
						if(access(nameOfFile ,F_OK) != -1)
						{
							//file exists
							write(1,"A file or archive file with that name already exists\n",53);
							return -1;
						}
						else
						{
							//create the file with the name we read and open it
							fd2 = open(nameOfFile, O_RDWR|O_CREAT, 0644);
							if(fd2 == -1)
							{
								write(1,"open() has failed and returned an error...\n",43);
								return -1;
							}
							else
							{
                                //write the files data   
								fileCounter = 0;
								while(fileCounter < header.file_size[i])
								{
									read(fd, &c, 1);
									bytesWritten = write(fd2,&c,1);
									if(bytesWritten == -1) return -1;
									fileCounter++;
								}
								close(fd2);
							}
						}
					}
                    if(header.next == header.eop) break;
                    //read the next header
					bytesRead = read(fd,&header,sizeof(hdr));
					if(bytesRead == -1) return -1;	
				}
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
        hdr header;
        bytesRead = read(fd,&header,sizeof(hdr));
        if(bytesRead == -1)
        {
            write(1,"read() has failed and returned an error...\n",43);
			close(fd);
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

int main(int argc, char **argv)
{
    if(argc == 2)
    {
		if(extractArchive(argv[1]) != -1)
        {
            //file is validated
			write(1, "File(s) successfully extracted\n", 31);
            return 0;
        }
        else
        {
            //file failed to validate
            write(1, "Failed to extract files from archive.\n", 38);
            return -1;
        }
    }
	else
	{
	    //not enough arguments given
        write(1,"usage: utar <archive-file-name>\n",32);
        return 0;	
	}
}
