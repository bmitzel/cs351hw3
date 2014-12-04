#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define DEBUG FALSE

using namespace std;

/* The pipe for parent-to-child communications */
int parentToChildPipe[2];

/* The pipe for the child-to-parent communication */
int childToParentPipe[2];

/* The read end of the pipe */
#define READ_END 0

/* The write end of the pipe */
#define WRITE_END 1

/* The maximum size of the array of hash programs */
#define HASH_PROG_ARRAY_SIZE 6

/* The maximum length of the hash value */
#define HASH_VALUE_LENGTH 1000

/* The maximum length of the file name */
#define MAX_FILE_NAME_LENGTH 1000

/* The array of names of hash programs */
const string hashProgs[] = {"md5sum", "sha1sum", "sha224sum", "sha256sum", "sha384sum",
		"sha512sum"};

/**
 * The function called by a child
 * @param hashProgName - the name of the hash program
 */
void computeHash(const string& hashProgName)
{
	/* The hash value buffer */
	char hashValue[HASH_VALUE_LENGTH];
	
	/* The received file name string */
	char fileNameRecv[MAX_FILE_NAME_LENGTH];
	
	/* Fill the buffer with 0's */
	memset(fileNameRecv, (char)NULL, MAX_FILE_NAME_LENGTH);	
	
	/* Now, lets read a message from the parent */
	if (read(parentToChildPipe[READ_END], fileNameRecv, sizeof(fileNameRecv)) < 0)
	{
		perror("read");
		exit(-1);
	}
#if DEBUG
	fprintf(stderr, "(DEBUG) fileNameRecv == %s\n", fileNameRecv);
#endif
	
	/* Glue together a command line <PROGRAM NAME>. 
 	 * For example, sha512sum fileName.
 	 */
	string cmdLine(hashProgName);
	cmdLine += " ";
	cmdLine += fileNameRecv;
#if DEBUG
	cerr << "(DEBUG) cmdLine == " << cmdLine << endl;
#endif
	
	/* Open the pipe to the program specified in cmdLine and save the ouput into hashValue */
	FILE* progOutput = popen(cmdLine.c_str(), "r");

	if (!progOutput)
	{
		perror("popen");
		exit(-1);
	}
		
	/* Reset the value buffer */
	memset(hashValue, (char)NULL, HASH_VALUE_LENGTH);

	if (fread(hashValue, sizeof(char), sizeof(char) * HASH_VALUE_LENGTH, progOutput) < 0)
	{
		perror("fread");
		exit(-1);
	}
#if DEBUG
	fprintf(stderr, "(DEBUG) hashValue == %s\n", hashValue);
#endif
		
	/* Send a string to the parent */
	if (write(childToParentPipe[WRITE_END], hashValue, sizeof(hashValue)) < 0)
	{
		perror("write");
		exit(-1);
	}

	/* The child terminates */
	exit(0);
} /* computeHash() */

/**
 * Begins program execution
 * @param argc - The number of command line arguments
 * @param argv - An array of C strings containing the command line arguments
 * @return The program exit code
 */
int main(int argc, char* argv[])
{
	/* Check for errors */
	if (argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]); 
		exit(-1);
	}	
	
	/* Save the name of the file */
	string fileName(argv[1]);
	
	/* Check if the file exists */
	if (FILE* test = fopen(fileName.c_str(), "r"))
	{
		/* The process id */
		pid_t pid;
		
		/* Run a program for each type of hash algorithm */
		for (int hashAlgNum = 0; hashAlgNum < HASH_PROG_ARRAY_SIZE; ++hashAlgNum)
		{
			/* Create a parent-to-child pipe */
			if (pipe(parentToChildPipe) < 0)
			{
				perror("pipe");
				exit(-1);
			}

			/* Create a child-to-parent pipe */
			if (pipe(childToParentPipe) < 0)
			{
				perror("pipe");
				exit(-1);
			}

			/* Fork a child process and save the id */
			if ((pid = fork()) < 0)
			{
				perror("fork");
				exit(-1);
			}
			else if (pid == 0) /* I am a child */
			{
				/* Close the unused write end of the parent-to-child pipe */
				if (close(parentToChildPipe[WRITE_END]) < 0)
				{
					perror("close");
					exit(-1);
				}

				/* Close the unused read end of the child-to-parent pipe */
				if (close(childToParentPipe[READ_END]) < 0)
				{
					perror("close");
					exit(-1);
				}

				/* Compute the hash */
				computeHash(hashProgs[hashAlgNum]);
			}
			
			/* I am the parent */

			/* Close the unused read end of the parent-to-child pipe */
			if (close(parentToChildPipe[READ_END]) < 0)
			{
				perror("close");
				exit(-1);
			}

			/* Close the unused write end of the child-to-parent pipe */
			if (close(childToParentPipe[WRITE_END]) < 0)
			{
				perror("close");
				exit(-1);
			}

			/* The buffer to hold the string received from the child */
			char hashValue[HASH_VALUE_LENGTH];
			
			/* Reset the hash buffer */
			memset(hashValue, (char)NULL, HASH_VALUE_LENGTH);

			/* Send the filename to the child */
#if DEBUG
			cerr << "(DEBUG) fileName == " << fileName << endl;
#endif
			if (write(parentToChildPipe[WRITE_END], fileName.c_str(), fileName.length() + 1) < 0)
			{
				perror("write");
				exit(-1);
			}

			/* Read the hash value sent by the child */
			if (read(childToParentPipe[READ_END], hashValue, sizeof(hashValue)) < 0)
			{
				perror("read");
				exit(-1);
			}

			/* Print the hash value */
			fprintf(stdout, "%s HASH VALUE: %s\n", hashProgs[hashAlgNum].c_str(), hashValue);
			fflush(stdout);

			/* Wait for the program to terminate */
			if (wait(NULL) < 0)
			{
				perror("wait");
				exit(-1);
			}
		} /* for */
	} /* if */
	else
	{
		perror("fopen");
		exit(-1);
	}
	
	return 0;
} /* main() */
