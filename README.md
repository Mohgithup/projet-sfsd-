# File Management System in C

This project is a file management system implemented in C. It allows users to manage files and records using different organizational modes (contiguous and chained) and internal sorting modes (sorted and unsorted). The system supports operations such as file creation, record insertion, deletion, searching, and defragmentation.

## Features

- **File Creation**: Create files with metadata including block size, record size, and organizational modes.
- **Record Management**:
  - Insert new records into files.
  - Search for records by ID.
  - Delete records logically or physically.
- **Defragmentation**: Compact files to remove empty spaces and maintain order.
- **Metadata Management**: Display and update metadata for all files.
- **Memory Management**: Initialize secondary memory, compact memory, and clear memory.

## Data Structures

- **TProduit**: Represents a product with fields for ID, name, price, and a flag for logical deletion.
- **TBloc**: Represents a block of records, containing an array of `TProduit` and metadata about the block.
- **Metadata**: Contains information about a file, such as filename, block size, record size, and organizational modes.
- **TableAllocation**: Tracks the state of blocks (free or occupied) in the file.

## Functions

- **File Operations**:
  - `creerFichier`: Creates a new file with specified metadata.
  - `Ouvrir` and `Fermer`: Open and close files.
  - `LireBloc` and `EcrireBloc`: Read and write blocks to/from a file.
  - `AllouerBloc`: Allocates a new block in the file.

- **Record Operations**:
  - `InsererEnregistrement`: Inserts a new record into a file.
  - `RechercherEnregistrement`: Searches for a record by ID.
  - `SupprimerEnregistrement`: Deletes a record by ID.
  - `Compactage`: Compacts records within a file to remove empty spaces.

- **Memory Management**:
  - `initialiser_systeme`: Initializes the secondary memory with a specified number of blocks and block size.
  - `vider`: Frees all allocated memory and resets the system.

- **Metadata Management**:
  - `add_metadata`: Adds metadata for a new file.
  - `display_metadata`: Displays metadata for all files.

## Usage

1. **Initialize the System**:
   - Use the `initialiser_systeme` function to initialize the secondary memory with the desired mode (contiguous or chained), number of blocks, and block size.

2. **Create a File**:
   - Use the `creerFichier` function to create a new file. Specify the filename, number of records, and organizational modes.

3. **Insert Records**:
   - Use the `InsererEnregistrement` function to insert new records into a file. Provide the product details (ID, name, price).

4. **Search for Records**:
   - Use the `RechercherEnregistrement` function to search for a record by its ID.

5. **Delete Records**:
   - Use the `SupprimerEnregistrement` function to delete a record by its ID. You can choose between logical and physical deletion.

6. **Defragment Files**:
   - Use the `Compactage` function to defragment a file, removing empty spaces and maintaining order.

7. **Display Metadata**:
   - Use the `display_metadata` function to view metadata for all files.

8. **Compact Memory**:
   - Use the `Compactage` function to compact the secondary memory, freeing up unused blocks.

9. **Clear Memory**:
   - Use the `vider` function to clear all files and reset the system.

## Example

```c
int main() {
    // Initialize the system with 100 blocks of size 512 bytes in contiguous mode
    initialiser_systeme(0, 100, 512);

    // Create a new file
    Metadata meta;
    FILE *file = creerFichier(&meta);

    // Insert a new record
    TProduit produit = {1, "Laptop", 999.99, 0};
    InsererEnregistrement(file, &meta, &produit, sizeof(TProduit));

    // Search for a record
    int block_number = RechercherEnregistrement(file, &meta, 1);
    if (block_number != -1) {
        printf("Record found in block %d\n", block_number);
    }

    // Delete a record
    SupprimerEnregistrement(file, &meta, 1);

    // Compact the file
    Compactage(file, &meta);

    // Display metadata
    display_metadata();

    // Close the file
    Fermer(file);

    return 0;
}