#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSAGE_LENGTH 100
#define FILE_NAME_LENGTH 100
#define FILE_SIZE_LENGTH 12
#define FILE_NAME_OFFSET 0
#define FILE_SIZE_OFFSET 124
#define FILE_TYPE_OFFSET 156
#define MAGIC_OFFSET 257
#define MAGIC_SIZE 5
#define TAR_MAGIC "ustar"
#define BLOCK_SIZE 512

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
	}
	exit(exit_code);
}

int block_is_zero(char *block) {
	int comparison_result;
	for (int i = 0; i < BLOCK_SIZE; i++) {
		comparison_result = (block[i] == '\0');
		if (!comparison_result) 
			break;
	}
	return comparison_result;
}

int ends_with_tar(char *str) {
	const char suffix[] = ".tar";
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
	char err_message[MAX_MESSAGE_LENGTH];
	FILE *archive = NULL;
	const int number_of_specified_files = argc - number_of_options - 2;
	char *specified_files[number_of_specified_files];
	int list_mode = 0;
	int verbose_mode = 0;
	int extract_mode = 0;
	
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
					if (arg_index + 1 >= argc)
						exit_with_code(2, "No archive file specified");
					archive = fopen(argv[++arg_index], "r");
					if (archive == NULL)
						exit_with_code(2, "No archive file specified");
					break;
				case 't':
					if (extract_mode)
						exit_with_code(2, "Cannot specify -t and -x at the same time");
					list_mode = 1;
					for (int i = 1, j = 0; i < argc; i++) {
						if ((argv[i][0] != '-' || strlen(argv[i]) != 2)) {
							specified_files[j++] = argv[i];	
						} else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
							i++;
						}
					}
					break;
				case 'v':
					verbose_mode = 1;
					break;
				case 'x':
					if (list_mode)
						exit_with_code(2, "Cannot specify -t and -x at the same time");
					extract_mode = 1;
					for (int i = 1, j = 0; i < argc; i++) {
						if ((argv[i][0] != '-' || strlen(argv[i]) != 2)) {
							specified_files[j++] = argv[i];	
						} else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
							i++;
						}
					}
					break;
				default:
					if (archive != NULL)
						fclose(archive);
					sprintf(err_message, "Unknown option '%s'", argv[arg_index]);
					exit_with_code(2, err_message);
			}
		}
	}

	/* reading the archive */
	char block[BLOCK_SIZE];
	char filename[FILE_NAME_LENGTH];
	char filetype;
	char magic[MAGIC_SIZE];
	char size_str[FILE_SIZE_LENGTH];
	unsigned long long file_size;
	int number_of_blocks;
	FILE *file_to_extract = NULL;
	int extract_this_file;
	int block_read;

	while (fread(block, 1, BLOCK_SIZE, archive) == BLOCK_SIZE) {
		if (block[0] == '\0')				/* end of archive */
			break;

		/* reading the header */
		strncpy(filename, block + FILE_NAME_OFFSET, FILE_NAME_LENGTH);
		strncpy(size_str, block + FILE_SIZE_OFFSET, FILE_SIZE_LENGTH);
		strncpy(magic, block + MAGIC_OFFSET, MAGIC_SIZE);
		filetype = block[FILE_TYPE_OFFSET];
		file_size = strtol(size_str, NULL, 8);
		number_of_blocks = 1 + (file_size - 1) / BLOCK_SIZE;
		extract_this_file = 0;

		/* checking the magic if file is an archive */
		if (strncmp(magic, TAR_MAGIC, MAGIC_SIZE) != 0) {
			fprintf(stderr, "mytar: This does not look like a tar archive\n");
			exit_code = 2;
			goto error_occurred;
		}

		/* checking the file type */
		if (filetype != '0') {				/* 0 signifies regular file */
			sprintf(err_message, "Unsupported header type: %d", (int)filetype);
			exit_code = 2;
			goto cleanup;
		}
		
		if (list_mode) {
			/* -t option: checking if the file was specified */
			if (number_of_specified_files > 0) {
				for (int i = 0; i < number_of_specified_files; i++) {
					if (specified_files[i] != NULL && strcmp(filename, specified_files[i]) == 0) {
						specified_files[i] = NULL;
						printf("%s\n", filename);
						break;
					}
				}
			}

			/* -t option: no files to list were specified */
			else printf("%s\n", filename);
		}

		if (extract_mode) {
			/* -x option: checking if the file was specified */
			if (number_of_specified_files > 0) {
				for (int i = 0; i < number_of_specified_files; i++) {
					if (specified_files[i] != NULL && strcmp(filename, specified_files[i]) == 0) {
						specified_files[i] = NULL;
						extract_this_file = 1;
						break;
					}
				}
			}

			/* -x option: no files to extract were specified; extracting all files */
			else extract_this_file = 1;
		}

		if (extract_this_file) {
			/* -x option: creating the file to extract */
			file_to_extract = fopen(filename, "w");
			if (file_to_extract == NULL) {
				fprintf(stderr, "mytar: Cannot create file %s\n", filename);
				sprintf(err_message, "Error is not recoverable: exiting now");
				exit_code = 2;
				goto cleanup;
			} 
			if (verbose_mode)
				printf("%s\n", filename);
		}

		/* reading the file content */
		for (int i = 0; i < number_of_blocks; i++) {
			block_read = fread(block, 1, BLOCK_SIZE, archive);
			int bytes_to_write = (i == number_of_blocks - 1) ? (file_size - i * BLOCK_SIZE) : BLOCK_SIZE;
			bytes_to_write = (block_read < bytes_to_write) ? block_read : bytes_to_write;
			if (extract_this_file && file_to_extract != NULL && bytes_to_write > 0) {
				/* -x option: writing the file content */
				fwrite(block, 1, bytes_to_write, file_to_extract);
			}
			if (block_read != BLOCK_SIZE) {
				fprintf(stderr, "mytar: Unexpected EOF in archive\n");
				sprintf(err_message, "Error is not recoverable: exiting now");
				exit_code = 2;
				if (file_to_extract != NULL)
					fclose(file_to_extract);
				goto cleanup;
			}
		}
		if (file_to_extract != NULL) {
			fclose(file_to_extract);
			file_to_extract = NULL;
		}
	}

	/* checking for a lone zero block at the end of the archive */
	if (block_is_zero(block)) {
		if (fread(block, 1, BLOCK_SIZE, archive) != BLOCK_SIZE || !block_is_zero(block))
			fprintf(stderr, "mytar: A lone zero block at %ld\n", ftell(archive)/BLOCK_SIZE);
	}

	/* -t -x options: checking if all files were found */
	for (int i = 0; i < number_of_specified_files; i++) {
		if (specified_files[i] != NULL) {
			fprintf(stderr, "mytar: %s: Not found in archive\n", specified_files[i]);
			exit_code = 2;
		}
	}

error_occurred:
	if (exit_code)
		sprintf(err_message, "Exiting with failure status due to previous errors");

cleanup:
	if (archive != NULL)
		fclose(archive);	

	exit_with_code(exit_code, err_message);
}
