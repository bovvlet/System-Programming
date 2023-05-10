#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userfs.h"

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

enum {
    BLOCK_SIZE = 512,
    MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
    /** Block memory. */
    char *memory;
    /** How many bytes are occupied. */
    int occupied;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
    int idx;
    int block_size;
};

struct file {
    /** Double-linked list of file blocks. */
    struct block *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    struct block *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /* PUT HERE OTHER MEMBERS */
    int planed_to_delete;
    int deleted;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
    struct file *file;

    /* PUT HERE OTHER MEMBERS */
    struct block *block_node;
    int block_offset;
    int cnt_flags;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

void assign_error_code(enum ufs_error_code u_e_c) {
	// Assigns the specified error code to the global variable ufs_error_code
	ufs_error_code = u_e_c;
}

enum ufs_error_code ufs_errno() { 
	// Returns the current value of the ufs_error_code
	return ufs_error_code; 
}

struct file *new_file(const char *filename) {
    // Allocates memory for a new file
    struct file *file = malloc(sizeof(struct file));
    if (file) {
        // Copies the filename into the file name attribute
        file->name = strdup(filename);
		if (file->name) {
			// Initializes the file attributes
			file->block_list = NULL;
			file->last_block = NULL;
			file->refs = 0;
			file->prev = NULL;
			file->next = file_list;
			file->planed_to_delete = 0;
			file->deleted = 0;
		} else {
			// Frees the allocated memory and assigns UFS_ERR_NO_MEM error code if strdup fails
			free(file);
			assign_error_code(UFS_ERR_NO_MEM);
			return NULL;
		}
	}
	else {
		// Assigns UFS_ERR_NO_MEM error code if memory allocation fails
		assign_error_code(UFS_ERR_NO_MEM);
        return NULL;
	}
	
    // Returns the new file
    return file;
}

int ufs_open(const char *filename, int cnt_flags) {
    // Find the file with the given filename
    struct file *file = file_list;
	while (file) {
        if (!file->planed_to_delete && !strcmp(file->name, filename)) {
            break;
        }
		file = file->next;
    }

    if (!file) {
        // If the file does not exist
        if ((cnt_flags & UFS_CREATE) != UFS_CREATE) {
            // Check if UFS_CREATE flag is not set
			assign_error_code(UFS_ERR_NO_FILE);
			return -1;
		} else {
            // Create a new file
			file = new_file(filename);
			if (!file) {
				return -1;
			}

			// Add the new file to the file list
			if (file_list) {
				file_list->prev = file;
			}
			file_list = file;
		}
	}

    // Find an available file descriptor slot
    int fd = -1;
	for (int i = 0; i < file_descriptor_count; i++) {
		if (!file_descriptors[i]) {
			fd = i;
			break;
		}
	}

	// If no available slot is found, use the next index
	if (fd == -1) {
		fd = file_descriptor_count;
	}

    // Increase the file descriptor count if necessary and resize the file descriptors array
    if (fd == file_descriptor_count) {
    	file_descriptor_count++;
		if (fd >= file_descriptor_capacity) {
			// Calculate the new capacity for the file descriptors array
			int new_capacity = (!file_descriptor_capacity) ? 1 : file_descriptor_capacity * 2;

			// Reallocate memory for the file descriptors array with the new capacity
			struct filedesc** new_descriptors = realloc(file_descriptors, new_capacity * sizeof(struct filedesc *));
			if (new_descriptors) {
				// Initialize the newly allocated memory with zeros
				memset(new_descriptors + file_descriptor_capacity, 0, (new_capacity - file_descriptor_capacity) * sizeof(struct filedesc *));
				file_descriptors = new_descriptors;
				file_descriptor_capacity = new_capacity;
			} else {
				// Assign UFS_ERR_NO_MEM error code if memory reallocation fails
				assign_error_code(UFS_ERR_NO_MEM);
				return -1;
			}
		}
	}

    // Create a new file descriptor for the file
    struct filedesc *filedesc = malloc(sizeof(struct filedesc));
    if (filedesc) {
        // Initialize the file descriptor attributes
        filedesc->file = file;
        filedesc->block_node = file->block_list;
        filedesc->block_offset = 0;
        filedesc->cnt_flags = cnt_flags;
    } else {
        assign_error_code(UFS_ERR_NO_MEM);
		return -1;
	}

    // Assign the file descriptor to the file descriptors array at the corresponding index
    file_descriptors[fd] = filedesc;
    file->refs++;

    return fd;
}

struct block *new_block_node() {
    // Allocates memory for a new block node
    struct block *block_node = malloc(sizeof(struct block));
    if (block_node) {
		// Initializes the block node attributes
		block_node->memory = malloc(BLOCK_SIZE);
		block_node->occupied = 0;
		block_node->next = NULL;
		block_node->prev = NULL;
		block_node->idx = 0;
		block_node->block_size = BLOCK_SIZE;
	} else {
		// Assigns UFS_ERR_NO_MEM error code if memory allocation fails
		assign_error_code(UFS_ERR_NO_MEM);
        return NULL;
	}
	
    // Returns the new block node
    return block_node;
}

ssize_t ufs_write(int fd, const char *buf, size_t size) {
    ssize_t bytes_cnt = 0;

	// Check if the file descriptor is valid
    if (fd < 0 || fd >= file_descriptor_count || !file_descriptors[fd]) {
        assign_error_code(UFS_ERR_NO_FILE);
        return -1;
    }

    // Get the file descriptor and check if it has write permissions
    struct filedesc *filedesc = file_descriptors[fd];
    if (filedesc->cnt_flags & UFS_READ_ONLY) {
        assign_error_code(UFS_ERR_NO_PERMISSION);
        return -1;
    }

    // Get the file associated with the file descriptor
    struct file *file = filedesc->file;

    // Create a new block if the file doesn't have a block list
    if (!file->block_list) {
        file->block_list = new_block_node();
        if (!file->block_list) {
            return -1;
        }
        file->last_block = file->block_list;
    }

    // Get the current block node from the file descriptor
    struct block *block_node = filedesc->block_node;
    if (!block_node) {
        block_node = filedesc->block_node = file->block_list;
    }

    // Check if writing the data would exceed the maximum file size
    if (filedesc->block_offset + size + filedesc->block_node->idx * BLOCK_SIZE > MAX_FILE_SIZE) {
        assign_error_code(UFS_ERR_NO_MEM);
        return -1;
    }

    int offset = filedesc->block_offset;

    // Write the data to the blocks
    while (size) {
        if (offset == BLOCK_SIZE) {
            // Create a new block node if the current block is full
            struct block *_block_node = new_block_node();
            if (!_block_node) {
                return -1;
            }

            _block_node->idx = block_node->idx + 1;
            _block_node->prev = block_node;
            if (block_node == file->last_block) {
                file->last_block = _block_node;
            }
            block_node->next = _block_node;
            block_node = _block_node;

            offset = 0;
        }

        int writable_bytes = BLOCK_SIZE - offset;
        if (writable_bytes > size) {
            writable_bytes = size;
        }

        // Copy the data to the block
        memcpy(block_node->memory + offset, buf + bytes_cnt, writable_bytes);
        offset += writable_bytes;
        bytes_cnt += writable_bytes;
        size -= writable_bytes;

        // Update the occupied size of the block if necessary
        if (offset > block_node->occupied) {
            block_node->occupied = offset;
        }
    }

    // Update the block node and offset in the file descriptor
    filedesc->block_node = block_node;
    filedesc->block_offset = offset;

    return bytes_cnt;
}

ssize_t ufs_read(int fd, char *buf, size_t size) {
    ssize_t bytes = 0;
    
	// Check if the file descriptor is valid
    if (fd < 0 || fd >= file_descriptor_count || !file_descriptors[fd]) {
        assign_error_code(UFS_ERR_NO_FILE);
        return -1;
    }

    // Get the file descriptor and check if it has read permissions
    struct filedesc *filedesc = file_descriptors[fd];
    if (filedesc->cnt_flags & UFS_WRITE_ONLY) {
        assign_error_code(UFS_ERR_NO_PERMISSION);
        return -1;
    }

    // Get the current block node from the file descriptor
    struct block *block_node = filedesc->block_node;
    if (!block_node) {
        block_node = filedesc->block_node = filedesc->file->block_list;
    }

    int offset = filedesc->block_offset;

    // Read data from the blocks
    while (block_node && size) {
        if (offset == BLOCK_SIZE) {
            block_node = block_node->next;
            offset = 0;
        }

        if (!(block_node->occupied - offset)) {
            break;
        } 
		
		int place_holder = MIN(block_node->occupied - offset, size);

        // Copy the data from the block to the buffer
        memcpy(buf + bytes, block_node->memory + offset, place_holder);
        bytes += place_holder;
        offset += place_holder;
        size -= place_holder;
    }

    // Update the block node and offset in the file descriptor
    filedesc->block_offset = offset;
    filedesc->block_node = block_node;

    return bytes;
}

int ufs_close(int fd) {
    struct filedesc *filedesc;
    // Check if the file descriptor is out of range or not associated with a file
    if (fd < 0 || fd >= file_descriptor_count || !file_descriptors[fd]) {
        assign_error_code(UFS_ERR_NO_FILE);
		return -1;
    }
	filedesc = file_descriptors[fd];

    struct file *file = filedesc->file;
    // Decrease the reference count of the file and check if it's planned for deletion
    if (!(--file->refs) && file->planed_to_delete) {
		// Adjust the linked list pointers to remove the file from the list
		if (file->prev) {
            file->prev->next = file->next;
		}
        else {
            file_list = file->next;
		}

        if (file->next) {
			file->next->prev = file->prev;
		}

		// Delete the file and free its resources
		ufs_delete(file->name);
		free(file);
    }

    // Free the file descriptor and set it to NULL
    free(filedesc);
    file_descriptors[fd] = NULL;

    return 0;
}

int ufs_delete(const char *filename) {
    struct file *file = file_list;
    // Find the file with the matching name in the file list
	while (file) {
        if (!file->planed_to_delete && !strcmp(file->name, filename)) {
            break;
        }
		file = file->next;
    }

    // If the file is not found, return an error
    if (!file) {
        assign_error_code(UFS_ERR_NO_FILE);
        return -1;
    }

    // Check if there are any references to the file
    if (file->refs > 0) {
        // Set the planned deletion flag for the file
        file->planed_to_delete = 1;
    } else {
        // Remove the file from the list by adjusting the linked list pointers
        if (file->prev) {
            file->prev->next = file->next;
		} else {
            file_list = file->next;
		}

        if (file->next) { 
			file->next->prev = file->prev;
		}

        // Delete the blocks associated with the file and free the resources
		struct block *block_to_delete = file->block_list;
        while (block_to_delete) {
			// Store the reference to the next block before freeing the current block
			struct block *next_block = block_to_delete->next;

			// Free the memory allocated for the block's data
			free(block_to_delete->memory);

			// Free the block node itself
			free(block_to_delete);

			// Move to the next block node
			block_to_delete = next_block;
    	}
        free(file->name);
        free(file);
    }

    return 0;
}