#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage() {
	printf("Usage: mytar [options] [file...]\n");
	printf("Options:\n");
	printf("  -h         : display this message and exit\n");
	printf("  -f <file>  : use archive file <file>\n");
	printf("  -t [files] : list archive contents\n");
	printf("  -v         : verbose mode\n");
	printf("  -x         : extract archive contents\n");
}

void exit_with_code(int exit_code, char *message) {
	if (exit_code != 0) {
		if (message != NULL)
			fprintf(stderr, "mytar: %s\n", message);
		// printUsage();
	}
	exit(exit_code);
}

int block_is_zero(char *block) {
	int comparison_result;
	for (int i = 0; i < 512; i++) {
		comparison_result = (block[i] == '\0');
		if (!comparison_result) 
			break;
	}
	return comparison_result;
}

int ends_with_tar(char *str)
{
	char suffix[] = ".tar";
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
        	return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int main(int argc, char *argv[]) {
	int number_of_options = 0;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && strlen(argv[i]) == 2)
			number_of_options++;
	}
	if (number_of_options < 1)
		exit_with_code(2, "need at least one option");

	int exit_code = 0;
	char err_message[256];
	FILE *archive = NULL;
	int number_of_files_to_list = argc - number_of_options - 2;
	char *files_to_list[number_of_files_to_list];
	
	/* parsing options */
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		if (argv[arg_index][0] == '-' && strlen(argv[arg_index]) == 2) {
			char option = argv[arg_index][1];
			switch (option) {
				case 'h':
					print_usage();
					exit_with_code(0, NULL);
					break;
				case 'f':
					for (int i = 1; i < argc; i++) {
						if (ends_with_tar(argv[i])) {
							archive = fopen(argv[i], "r");
							break;
						}
					}
					break;
				case 't':
					for (int i = 1, j = 0; i < argc; i++) {
						if ((argv[i][0] != '-' || strlen(argv[i]) != 2) && !ends_with_tar(argv[i]))
							files_to_list[j++] = argv[i];	
					}
					break;
				case 'v':
					/* TODO 
					 * only with option -x, else ignored 
					 */
					printf("Verbose mode...\n");
					exit_with_code(2, "Not implemented yet");
					break;
				case 'x':
					/* TODO */
					printf("Extracting archive...\n");
					exit_with_code(2, "Not implemented yet");
					break;
				default:
					if (archive != NULL)
						fclose(archive);
					sprintf(err_message, "Unknown option '%s'", argv[arg_index]);
					exit_with_code(2, err_message);
			}
		}
	}

	if (archive == NULL) 
		exit_with_code(2, "No archive file specified");
	
	/* reading the archive */
	char block[512];
	char filename[100];
	char filetype;
	char size_str[12];
	unsigned long long file_size;
	int number_of_blocks;

	while (fread(block, 1, 512, archive) == 512) {
		if (block[0] == '\0')				/* end of archive */
			break;

		/* reading the header */
		strncpy(filename, block, 100);			/* filename	is at offset   0, 100 bytes */
		strncpy(size_str, block + 124, 12);		/* size		is at offset 124,  12 bytes */
		filetype = block[156]; 				/* type 	is at offset 156,   1 byte  */
		file_size = strtol(size_str, NULL, 8);
		number_of_blocks = 1 + (file_size - 1) / 512;

		if (filetype != '0') {				/* 0 signifies regular file */
			sprintf(err_message, "Unsupported header type: %d\n", (int)filetype);
			exit_code = 2;
			goto cleanup;
		}

		/* -t option: checking if the file is in the list */
		if (number_of_files_to_list > 0) {
			for (int i = 0; i < number_of_files_to_list; i++) {
				if (files_to_list[i] != NULL && strcmp(filename, files_to_list[i]) == 0) {
					files_to_list[i] = NULL;
					printf("%s\n", filename);
					break;
				}
			}
		}

		/* -t option: no files to list were specified */
		else printf("%s\n", filename);

		/* reading the file content */
		for (int i = 0; i < number_of_blocks; i++) {
			if (fread(block, 1, 512, archive) != 512) {
				fprintf(stderr, "mytar: Unexpected EOF in archive\n");
				sprintf(err_message, "Error is not recoverable: exiting now\n");
				exit_code = 2;
				goto cleanup;
			}
		}
	}

	/* checking for a lone zero block at the end of the archive */
	if (block_is_zero(block)) {
		if (fread(block, 1, 512, archive) != 512 || !block_is_zero(block))
			fprintf(stderr, "mytar: A lone zero block at %ld\n", ftell(archive)/512);
	}

	/* -t option: checking if all files were found */
	for (int i = 0; i < number_of_files_to_list; i++) {
		if (files_to_list[i] != NULL) {
			fprintf(stderr, "mytar: %s: Not found in archive\n", files_to_list[i]);
			exit_code = 2;
		}
	}

	if (exit_code)
		sprintf(err_message, "Exiting with failure status due to previous errors\n");

cleanup:
	if (archive != NULL)
		fclose(archive);	

	exit_with_code(exit_code, err_message);
}
