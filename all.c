#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <windows.h>
#include <winuser.h>
#include <conio.h>
#include <io.h>
#define FB 5
#define boolean int
#define true 1
#define false 0
#define MAX_METADATA 100
#define VERT "\033[0;32m"
#define ROUGE "\033[0;31m"
#define RESET "\033[0m"
#define FILENAME_MAX_LENGTH 256
#define MAX_RETRY 3
#define MAX_FILES 100

typedef struct Produit
{
    int id;
    char nom[21];
    float prix;
    bool suppr;
} TProduit;

typedef struct TBloc
{
    TProduit t[FB];
    int ne;
    int next_bloc;
} TBloc;

typedef struct Metadata
{
    char filename[FILENAME_MAX_LENGTH];
    int block_size;
    int record_size;
    int first_block;
    int global_mode;   // 0: Contiguous, 1: Chained
    int internal_mode; // 0: Sorted, 1: Unsorted
} Metadata;

typedef struct Buffer
{
    TBloc bloc_1;
    TBloc bloc_2;
    TBloc bloc_3;
    TBloc bloc_4;
    TBloc bloc_5;
} Buffer;

typedef struct Index
{
    int etatbloc;
    int *enregistrement;
} TIndex;

typedef struct Tbloc {
    TProduit *tenrg;        // Pointer to an array of records (TProduit)
    struct Tbloc *next_bloc; // Pointer to the next block
} bloc;


typedef struct TableAllocation
{
    int *etats;   // tableau des états des blocs (0: libre, 1: occupé)
    int nb_blocs; // nombre total de blocs
} TableAllocation;

Metadata metadata_array[MAX_METADATA];
int metadata_count = 0;

void error_exit(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

// Function to initialize metadata
void initialize_metadata(Metadata *meta, const char *filename, int block_size, int record_size, int first_block, int global_mode, int internal_mode)
{
    strncpy(meta->filename, filename, FILENAME_MAX_LENGTH);
    meta->block_size = block_size;
    meta->record_size = record_size;
    meta->first_block = first_block;
    meta->global_mode = global_mode;
    meta->internal_mode = internal_mode;
}

// Function to add a new metadata entry
void add_metadata(const char *filename, int block_size, int record_size, int first_block, int global_mode, int internal_mode)
{
    if (metadata_count >= MAX_METADATA)
    {
        printf("Error: Metadata storage is full.\n");
        return;
    }
    initialize_metadata(&metadata_array[metadata_count], filename, block_size, record_size, first_block, global_mode, internal_mode);
    metadata_count++;
    printf("Metadata for file '%s' added successfully.\n", filename);
}

// Function to display all metadata
void display_metadata()
{
    printf("\n--- Metadata Table ---\n");
    printf("%-20s %-15s %-15s %-15s %-15s %-15s\n", "Filename", "Block Size", "Record Size", "First Block", "Global Mode", "Internal Mode");
    for (int i = 0; i < metadata_count; i++)
    {
        printf("%-20s %-15d %-15d %-15d %-15d %-15d\n",
               metadata_array[i].filename,
               metadata_array[i].block_size,
               metadata_array[i].record_size,
               metadata_array[i].first_block,
               metadata_array[i].global_mode,
               metadata_array[i].internal_mode);
    }
}

void Ouvrir(FILE **F, const char *nomFichier, const char *mode)
{
    *F = fopen(nomFichier, mode);
    if (!*F)
        error_exit("Error opening file");
}

void Fermer(FILE *F)
{
    if (F != NULL)
    {
        fclose(F);
        printf("Fichier fermé avec succès.\n");
    }
    else
    {
        printf("Aucun fichier à fermer (pointeur NULL).\n");
    }
}

void LireBloc(FILE *F, int i, TBloc *buffer) {
    fseek(F, sizeof(Metadata) + i * sizeof(TBloc), SEEK_SET);
    fread(buffer, sizeof(TBloc), 1, F);
}

void EcrireBloc(FILE *F, int i, TBloc *buffer)
{
    fseek(F, sizeof(Metadata) + i * sizeof(TBloc), SEEK_SET);
    fwrite(buffer, sizeof(TBloc), 1, F);
}

int AllouerBloc(FILE *F)
{
    if (F == NULL)
    {
        printf("Erreur : le fichier n'est pas ouvert.\n");
        exit(EXIT_FAILURE);
    }
    fseek(F, 0, SEEK_END);
    long tailleFichier = ftell(F);
    int numeroBloc = tailleFichier / sizeof(TBloc);
    TBloc blocVide = {.ne = 0};
    size_t result = fwrite(&blocVide, sizeof(TBloc), 1, F);
    if (result != 1)
    {
        perror("Erreur lors de l'allocation d'un nouveau Tbloc");
        exit(EXIT_FAILURE);
    }
    printf("Bloc %d alloué avec succès.\n", numeroBloc);
    return numeroBloc;
}

int LireEntete(Metadata *meta, int i)
{
    switch (i)
    {
    case 0:
        return meta->block_size;
    case 1:
        return meta->record_size;
    case 2:
        return meta->first_block;
    case 3:
        return meta->global_mode;
    case 4:
        return meta->internal_mode;
    default:
        printf("Erreur : indice invalide pour la lecture de l'entête.\n");
        exit(EXIT_FAILURE);
    }
}

void MAJEntete(Metadata *meta, int i, int val)
{
    switch (i)
    {
    case 0:
        meta->block_size = val;
        break;
    case 1:
        meta->record_size = val;
        break;
    case 2:
        meta->first_block = val;
        break;
    case 3:
        meta->global_mode = val;
        break;
    case 4:
        meta->internal_mode = val;
        break;
    default:
        printf("Erreur : indice invalide pour la mise à jour de l'entête.\n");
        exit(EXIT_FAILURE);
    }
}

bool initialiser_systeme(int mode, int nb_blocks, int block_size) {
    // Validate inputs
    if (mode != 0 && mode != 1) {
        printf("Error: Invalid mode. Use 0 for Contiguous or 1 for Chained.\n");
        return false;
    }

    if (nb_blocks <= 0 || block_size <= 0) {
        printf("Error: Number of blocks and block size must be greater than zero.\n");
        return false;
    }

    // Initialize the allocation table
    TableAllocation *table_allocation = malloc(sizeof(TableAllocation));
    if (!table_allocation) {
        printf("Error: Memory allocation failed for TableAllocation.\n");
        return false;
    }

    table_allocation->etats = malloc(nb_blocks * sizeof(int));
    if (!table_allocation->etats) {
        printf("Error: Memory allocation failed for block states.\n");
        free(table_allocation);
        return false;
    }
    memset(table_allocation->etats, 0, nb_blocks * sizeof(int)); // Set all blocks as free
    table_allocation->nb_blocs = nb_blocks;

    // Print initialization details
    printf("System initialized successfully.\n");
    printf("Mode: %s\n", mode == 0 ? "Contiguous" : "Chained");
    printf("Number of blocks: %d\n", nb_blocks);
    printf("Block size: %d bytes\n", block_size);

    // Clean up (if this is a temporary allocation for demonstration purposes)
    free(table_allocation->etats);
    free(table_allocation);

    return true;
}

void nettoyer_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        // Consume characters until the newline or EOF
    }
}

FILE *create_file(Metadata *meta)
{
    FILE *F = fopen(meta->filename, "wb+");
    if (!F)
        error_exit("Error creating file");

    // Write metadata as file header
    fwrite(meta, sizeof(Metadata), 1, F);

    // Initialize and write empty blocks
    TBloc empty_block = {.ne = 0, .next_bloc = -1};
    for (int i = 0; i < meta->block_size; i++)
    {
        fwrite(&empty_block, sizeof(TBloc), 1, F);
    }

    printf("File '%s' created successfully with %d blocks.\n", meta->filename, meta->block_size);
    return F;
}

typedef struct Ms
{
    TBloc *memoire;
    int taille;
} Ms;

typedef struct recherche
{
    int numeroBloc;
    int deplacement;
    bool trouve; // Ajout d'un booléen pour indiquer si l'enregistrement a été trouvé
} recherche;

// Function to initialize the allocation table
TIndex *initialisation(int nombre_total_bloc, int taille_bloc)
{
    TIndex *allocation_table = (TIndex *)malloc(nombre_total_bloc * sizeof(TIndex));
    if (!allocation_table)
    {
        printf("Error: Memory allocation failed.\n");
        return NULL;
    }

    for (int i = 0; i < nombre_total_bloc; i++)
    {
        allocation_table[i].etatbloc = 0;
        allocation_table[i].enregistrement = (int *)malloc(taille_bloc * sizeof(int));
        if (!allocation_table[i].enregistrement)
        {
            printf("Error: Memory allocation failed for enregistrement.\n");
            return NULL;
        }
        for (int j = 0; j < taille_bloc; j++)
        {
            allocation_table[i].enregistrement[j] = 0;
        }
    }
    return allocation_table;
}

// Function to compact blocks in chained organization mode
void compactage_blocs(bloc **head_bloc, TIndex *table_allocation, int nombre_blocs, int taille_bloc) {
    bloc *current = *head_bloc;
    bloc *prev = NULL;

    for (int i = 0; i < nombre_blocs; i++) {
        if (table_allocation[i].etatbloc == 0) { // Check if the block is unused
            bloc *to_remove = NULL;

            // Traverse to the corresponding block
            int temp = i;
            while (current && temp-- > 0) {
                prev = current;
                current = current->next_bloc;
            }

            // Remove the block from the list
            if (current) {
                to_remove = current;
                if (prev) {
                    prev->next_bloc = current->next_bloc;
                } else {
                    *head_bloc = current->next_bloc;
                }
                current = prev ? prev->next_bloc : *head_bloc;

                // Free the memory for the removed block
                free(to_remove->tenrg); // Free the dynamically allocated `tenrg`
                free(to_remove);
            }
        } else {
            // Move to the next block
            prev = current;
            if (current) current = current->next_bloc;
        }
    }
}


// Function to compact records within blocks
void compactage_enregistrements(bloc *head_bloc, TIndex *table_allocation, int taille_bloc)
{
    bloc *current = head_bloc;
    TProduit *temp_storage = (TProduit *)malloc(taille_bloc * sizeof(TProduit));
    if (!temp_storage)
    {
        printf("Error: Memory allocation failed for temp_storage.\n");
        return;
    }

    while (current)
    {
        int index = 0;
        for (int i = 0; i < taille_bloc; i++)
        {
            if (table_allocation->enregistrement[i] == 1)
            {
                temp_storage[index++] = current->tenrg[i];
            }
        }
        for (int i = 0; i < index; i++)
        {
            current->tenrg[i] = temp_storage[i];
        }
        current = current->next_bloc;
    }

    free(temp_storage);
}

// Function to free the entire memory structure
void vider(bloc *head_bloc, TIndex *table_allocation, int nombre_blocs)
{
    bloc *current = head_bloc;
    while (current)
    {
        bloc *temp = current;
        current = current->next_bloc;
        free(temp->tenrg);
        free(temp);
    }

    for (int i = 0; i < nombre_blocs; i++)
    {
        free(table_allocation[i].enregistrement);
    }
    free(table_allocation);
}

// Function to check if storage is full
int plein(bloc *head_bloc, TIndex *table_allocation, int taille_bloc, int nombre_blocs, int nb_enreg_insert)
{
    bloc *current = head_bloc;
    int counter = 0;

    while (current)
    {
        current = current->next_bloc;
        counter++;
    }

    if (counter >= nombre_blocs)
    {
        return -1; // No free blocks available
    }

    for (int i = 0; i < taille_bloc; i++)
    {
        if (table_allocation[counter].enregistrement[i] == 0)
        {
            if (nb_enreg_insert <= (taille_bloc - i))
            {
                return 0; // Space available within current block
            }
        }
    }

    return 1; // Additional block required
}

bool lire_entier(int *value, const char *prompt) {
    char buffer[100];
    int temp;

    printf("%s: ", prompt);

    // Read a line of input
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return false; // Input error
    }

    // Attempt to parse an integer from the input
    if (sscanf(buffer, "%d", &temp) != 1) {
        printf("Invalid input. Please enter a valid integer.\n");
        return false;
    }

    *value = temp;
    return true;
}

void Lirebloc(FILE *F, int block_number, TBloc *buffer);
bool lire_entier(int *value, const char *prompt);
void Ouvrir(FILE **F, const char *filename, const char *mode);
// Fonction d'initialisation du fichier et de la table d'allocation
// InitialiserFichier :
// *Crée le fichier Initialise l'entête
// *Crée la table d'allocation
// *Initialise tous les blocs comme vides

void InitialiserFichier(const char *nomFichier, int nb_blocs, int taille_bloc)
{
    FILE *F;
    Ouvrir(&F, nomFichier, "wb+");

    // Initialize Metadata
    Metadata meta = {
        .block_size = taille_bloc,
        .record_size = sizeof(TProduit),
        .first_block = 1,
        .global_mode = 0,
        .internal_mode = 0};

    fwrite(&meta, sizeof(Metadata), 1, F);

    // Initialize TableAllocation
    TableAllocation table = {
        .etats = (int *)calloc(nb_blocs, sizeof(int)),
        .nb_blocs = nb_blocs};

    TBloc blocTable = {.ne = 0};
    memcpy(blocTable.t, table.etats, nb_blocs * sizeof(int));
    EcrireBloc(F, 0, &blocTable);

    // Initialize remaining blocks
    TBloc blocVide = {.ne = 0};
    for (int i = 1; i < nb_blocs; i++)
    {
        EcrireBloc(F, i, &blocVide);
    }

    free(table.etats);
    fclose(F);
}

int InsererEnregistrement(FILE *F, Metadata *meta, void *enreg, int taille_enreg)
{
    TBloc buffer;
    TableAllocation table;
    table.etats = (int *)malloc(meta->block_size * sizeof(int));

    // Read TableAllocation
    LireBloc(F, 0, &buffer);
    memcpy(table.etats, buffer.t, meta->block_size * sizeof(int));

    // Find a free block
    int bloc_libre = -1;
    for (int i = 1; i < meta->block_size; i++)
    {
        if (table.etats[i] == 0)
        {
            bloc_libre = i;
            break;
        }
    }

    if (bloc_libre == -1)
    {
        printf("No free block available\n");
        free(table.etats);
        return -1;
    }

    // Read the free block
    LireBloc(F, bloc_libre, &buffer);

    // Check for available space in the block
    if (buffer.ne >= FB)
    {
        printf("Block full\n");
        free(table.etats);
        return -1;
    }

    // Insert the record
    memcpy(&buffer.t[buffer.ne], enreg, taille_enreg);
    buffer.ne++;

    // Update the block
    EcrireBloc(F, bloc_libre, &buffer);

    // Update TableAllocation
    table.etats[bloc_libre] = 1;
    memcpy(buffer.t, table.etats, meta->block_size * sizeof(int));
    EcrireBloc(F, 0, &buffer);

    free(table.etats);
    return bloc_libre;
}

int RechercherEnregistrement(FILE *F, Metadata *meta, int id)
{
    TBloc buffer;
    TableAllocation table;
    table.etats = (int *)malloc(meta->block_size * sizeof(int));

    // Read TableAllocation
    LireBloc(F, 0, &buffer);
    memcpy(table.etats, buffer.t, meta->block_size * sizeof(int));

    // Search occupied blocks
    for (int i = 1; i < meta->block_size; i++)
    {
        if (table.etats[i] == 1)
        {
            LireBloc(F, i, &buffer);

            // Search records in the block
            for (int j = 0; j < buffer.ne; j++)
            {
                if (buffer.t[j].id == id)
                {
                    free(table.etats);
                    return i; // Return the block number
                }
            }
        }
    }

    free(table.etats);
    return -1; // Record not found
}

int SupprimerEnregistrement(FILE *F, Metadata *meta, int id)
{
    int Tbloc = RechercherEnregistrement(F, meta, id);
    if (Tbloc == -1)
    {
        return 0; // Record not found
    }

    TBloc buffer;
    LireBloc(F, Tbloc, &buffer);

    // Search and remove the record
    for (int i = 0; i < buffer.ne; i++)
    {
        if (buffer.t[i].id == id)
        {
            // Shift records
            for (int j = i; j < buffer.ne - 1; j++)
            {
                buffer.t[j] = buffer.t[j + 1];
            }
            buffer.ne--;
            EcrireBloc(F, Tbloc, &buffer);

            // Update TableAllocation if the block becomes empty
            if (buffer.ne == 0)
            {
                TableAllocation table;
                table.etats = (int *)malloc(meta->block_size * sizeof(int));
                LireBloc(F, 0, &buffer);
                memcpy(table.etats, buffer.t, meta->block_size * sizeof(int));
                table.etats[Tbloc] = 0;
                memcpy(buffer.t, table.etats, meta->block_size * sizeof(int));
                EcrireBloc(F, 0, &buffer);
                free(table.etats);
            }

            return 1; // Successful deletion
        }
    }

    return 0; // Record not found in the block
}

void Compactage(FILE *F, Metadata *meta)
{
    TBloc buffer;
    TableAllocation table;
    table.etats = (int *)malloc(meta->block_size * sizeof(int));

    // Read TableAllocation
    LireBloc(F, 0, &buffer);
    memcpy(table.etats, buffer.t, meta->block_size * sizeof(int));

    int bloc_destination = 1;
    int bloc_source = 1;

    while (bloc_source < meta->block_size)
    {
        while (bloc_source < meta->block_size && table.etats[bloc_source] == 0)
        {
            bloc_source++;
        }

        if (bloc_source < meta->block_size)
        {
            if (bloc_destination != bloc_source)
            {
                LireBloc(F, bloc_source, &buffer);
                EcrireBloc(F, bloc_destination, &buffer);
                table.etats[bloc_destination] = 1;
                table.etats[bloc_source] = 0;
            }
            bloc_destination++;
            bloc_source++;
        }
    }

    memcpy(buffer.t, table.etats, meta->block_size * sizeof(int));
    EcrireBloc(F, 0, &buffer);

    free(table.etats);
}

//-------------------------------------------------Predefined variables of the automation of record generation----------------------------------------------//
#define NAMES {"Laptop", "Smartphone", "Tablet", "Headphones", "Smartwatch",             \
               "Monitor", "Keyboard", "Mouse", "Printer", "Router",                      \
               "TV", "Camera", "Speaker", "Projector", "Microphone",                     \
               "External Hard Drive", "USB Flash Drive", "Charger", "Power Bank", "SSD", \
               "Gaming Console", "Drone", "VR Headset", "Graphics Card", "Processor",    \
               "Motherboard", "RAM", "Cooling Fan", "Power Supply", "Case",              \
               "Webcam", "Fitness Tracker", "E-reader", "Electric Toothbrush", "Shaver", \
               "Air Purifier", "Coffee Maker", "Blender", "Vacuum Cleaner", "Oven",      \
               "Refrigerator", "Washing Machine", "Dryer", "Dishwasher", "Microwave",    \
               "Air Conditioner", "Heater", "Water Filter", "Smart Light", "Smart Lock"}

#define PRIX {999.99, 799.99, 499.99, 199.99, 249.99, \
              299.99, 49.99, 29.99, 149.99, 89.99,    \
              1199.99, 499.99, 99.99, 699.99, 149.99, \
              89.99, 19.99, 29.99, 39.99, 99.99,      \
              499.99, 999.99, 399.99, 799.99, 399.99, \
              199.99, 89.99, 39.99, 69.99, 59.99,     \
              49.99, 129.99, 89.99, 49.99, 39.99,     \
              199.99, 129.99, 89.99, 499.99, 1199.99, \
              699.99, 599.99, 399.99, 299.99, 499.99, \
              899.99, 79.99, 49.99, 39.99, 129.99}
//-------------------------------------------------Predefined variables of the automation of record generation----------------------------------------------//

typedef struct
{
    int id;
    char nom[21];
    float prix;
    int supr;
} Tproduit;

typedef struct
{
    int etatbloc;
} Index;

typedef struct MetaData
{
    char FileName[256];
    int FileSize_Blocks;
    int FileSize_Records;
    int FirstBlock_Address;
    char GlobalOrganization_Mode[20];
    char InternalOrganization_Mode[20];
} TMetaData;

typedef struct
{
    TProduit *tenrg;
    int nbP;
    TIndex *allocation_table;
    TMetaData MetaData;
    int next_bloc;

} Tbloc;

// Function to initialize the allocation table
Index *Initialisation(int nombre_total_bloc)
{
    Index *allocation_table = (Index *)malloc(nombre_total_bloc * sizeof(Index));
    if (!allocation_table)
    {
        printf("Error: Memory allocation failed.\n");
        return NULL;
    }

    for (int i = 0; i < nombre_total_bloc; i++)
    {
        allocation_table[i].etatbloc = 0;
    }
    return allocation_table;
}

int find_free_block(FILE *Xname)
{
    Tbloc Buf;
    int size;
    fseek(Xname, sizeof(Tbloc), SEEK_SET);
    fread(&Buf, sizeof(Tbloc), 1, Xname);
    size = sizeof(Buf.allocation_table) / sizeof(Buf.allocation_table[0]);
    for (int i = 0; i < size; i++)
    {
        if (Buf.allocation_table[i].etatbloc == 0)
        {
            return i;
        }
    }
    return -1; // No free block available
}

// Function to compact blocks in chained organization mode
void Compactage_blocs(Tbloc head, int nombre_blocs)
{
    int current_index = 0;
    int next_index = -1;

    while (current_index < nombre_blocs)
    {
        if (head.allocation_table[current_index].etatbloc == 0)
        {
            // Skip over unused blocks
            next_index = head.next_bloc;
            head.allocation_table[current_index].etatbloc = next_index;
        }
        else
        {
            current_index = head.allocation_table[current_index].etatbloc;
        }
    }
}

// Function to free the entire memory structure
void Vider(Index *table_allocation, int nombre_blocs)
{
    int current_index = 0;

    while (current_index < nombre_blocs)
    {
        if (table_allocation[current_index].etatbloc != 0)
        {
            // Simulate freeing resources
            table_allocation[current_index].etatbloc = 0; // Assuming that freeing a block sets its state to 0
        }
        current_index++;
    }
    free(table_allocation);
}

// Function to check if storage is full
int Plein(Index *table_allocation, int nombre_blocs)
{
    int counter = 0;

    for (int i = 0; i < nombre_blocs; i++)
    {
        if (table_allocation[i].etatbloc != 0)
        {
            counter++;
        }
    }

    if (counter >= nombre_blocs)
    {
        return 1; // No free blocks available
    }

    return 0;
}


// Function Definitions
void create_file_metadata(FILE *Xname, Metadata *files, int *file_count, int max_files) {
    if (*file_count >= max_files) {
        printf("Error: Maximum number of files reached.\n");
        return;
    }

    Metadata new_file;
    int nbrProducts, nbrBlocks;

    // Gather file details
    printf("Enter file name: ");
    scanf("%s", new_file.filename);

    printf("Enter the number of records: ");
    scanf("%d", &nbrProducts);

    nbrBlocks = (nbrProducts + FB - 1) / FB; // Round up to full blocks
    new_file.block_size = nbrBlocks;
    new_file.record_size = nbrProducts;

    printf("Enter the global organization mode (0: Contiguous, 1: Chained): ");
    scanf("%d", &new_file.global_mode);

    printf("Enter the internal organization mode (0: Sorted, 1: Unsorted): ");
    scanf("%d", &new_file.internal_mode);

    // Find the first available block
    printf("Enter the first block index: ");
    scanf("%d", &new_file.first_block);

    // Add to metadata array
    files[*file_count] = new_file;
    (*file_count)++;

    printf("File '%s' created successfully with %d blocks.\n", new_file.filename, nbrBlocks);
}

void afficher_etat_memoire(Metadata *files, int nb_blocks, int block_size) {
    printf("=== État de la Mémoire ===\n");
    printf("Nombre total de blocs : %d\n", nb_blocks);
    printf("Taille de chaque bloc : %d\n", block_size);

    printf("\nFichiers :\n");
    for (int i = 0; i < nb_blocks; i++) {
        if (strlen(files[i].filename) > 0) {
            printf("Fichier %d :\n", i + 1);
            printf("  Nom : %s\n", files[i].filename);
            printf("  Taille du bloc : %d\n", files[i].block_size);
            printf("  Mode global : %s\n", files[i].global_mode == 1 ? "Contiguë" : "Chaînée");
        }
    }
}



// Function to update metadata
void Update_MetaFile(char dataFileName[], TMetaData metadata)
{
    FILE *metaFile = fopen(strcat(dataFileName, "_Meta"), "rb+");
    if (!metaFile)
    {
        perror("Error opening metadata file");
        return;
    }
    fwrite(&metadata, sizeof(TMetaData), 1, metaFile);
    fclose(metaFile);
    printf("Metadata updated successfully for '%s_Meta'.\n", dataFileName);
}

// Function to read metadata
void Read_MetaFile(char dataFileName[])
{
    FILE *metaFile = fopen(strcat(dataFileName, "_Meta"), "rb");
    if (!metaFile)
    {
        perror("Error opening metadata file");
        return;
    }
    TMetaData metadata;
    fread(&metadata, sizeof(TMetaData), 1, metaFile);
    printf("Metadata for %s_Meta:\n", dataFileName);
    printf("File Name: %s\n", metadata.FileName);
    printf("File Size (Records): %d\n", metadata.FileSize_Records);
    printf("File Size (Blocks): %d\n", metadata.FileSize_Blocks);
    printf("Global Organization Mode: %s\n", metadata.GlobalOrganization_Mode);
    printf("Internal Organization Mode: %s\n", metadata.InternalOrganization_Mode);
}

void Insert_Record(Tbloc *Buf, TProduit *Rec)
{

    int i = 0;
    TProduit tmp;
    // insert the record in the right position while maintaining the order//
    while (i < Buf->nbP)
    {
        if (Buf->tenrg[i].id > Rec->id)
        {
            tmp = Buf->tenrg[i];
            Buf->tenrg[i] = *Rec;
            *Rec = tmp;
        }
        i++;
    }
    //-----------------------------------------------------------------//
}

// Insert a new record into a file
void insert_record(FILE *Xname, char *file_name)
{
    Tbloc Buf, BufMeta, BufFileData;
    int size, found = 0, i;
    TProduit record;

    record.suppr = 0;
    printf("please introduce the product name\n");
    scanf(" %256s", record.nom);
    printf("please introduce the product id\n");
    scanf("%d", &record.id);
    printf("the product price\n");
    scanf("%f", &record.prix);

    size = sizeof(Buf.allocation_table) / sizeof(Buf.allocation_table[0]);
    fseek(Xname, sizeof(bloc), SEEK_SET);
    // Find the file metadata
    for (i = 0; i < size; i++)
    {
        if (Buf.allocation_table[i].etatbloc == 0)
        {
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fread(&BufMeta, sizeof(bloc), 1, Xname);
            if (strcmp(BufMeta.MetaData.FileName, file_name) == 0)
            {
                found = 1;
                break;
            }
        }
    }

    if (!found)
    {
        printf("Error: File '%s' not found.\n", file_name);
        return;
    }

    int current_block, new_block;

    if (strcpy(BufMeta.MetaData.InternalOrganization_Mode, "Unsorted"))
    {
        current_block = BufMeta.MetaData.FirstBlock_Address;
        fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
        fread(&BufFileData, sizeof(bloc), 1, Xname);
        while (BufFileData.next_bloc != -1)
        {
            fseek(Xname, BufFileData.next_bloc * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
        }
        if (BufFileData.nbP < FB)
        {
            BufFileData.tenrg[BufFileData.nbP] = record;
            BufFileData.nbP++;
            BufMeta.MetaData.FileSize_Records++;
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fwrite(&BufMeta, sizeof(bloc), 1, Xname);
        }
        else
        {
            new_block = find_free_block(Xname);
            if (new_block == -1)
            {
                printf("Error: No free blocks available.\n");
                return;
            }
            BufFileData.next_bloc = new_block;
            fseek(Xname, new_block * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
            BufFileData.tenrg[0] = record;
            BufFileData.nbP++;
            BufFileData.next_bloc = -1;
            fseek(Xname, new_block * sizeof(bloc), SEEK_SET);
            fwrite(&BufFileData, sizeof(bloc), 1, Xname);
            BufMeta.MetaData.FileSize_Records++;
            BufMeta.MetaData.FileSize_Blocks++;
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fwrite(&BufMeta, sizeof(bloc), 1, Xname);
        }
    }
    else
    {
        i = 0;
        current_block = BufMeta.MetaData.FirstBlock_Address;
        fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
        fread(&BufFileData, sizeof(bloc), 1, Xname);
        Insert_Record(&BufFileData, &record);
        while (BufFileData.next_bloc != -1)
        {
            fseek(Xname, BufFileData.next_bloc * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
            Insert_Record(&BufFileData, &record);
        }

        if (BufFileData.nbP < FB)
        {
            BufFileData.tenrg[BufFileData.nbP] = record;
            BufFileData.nbP++;
            BufMeta.MetaData.FileSize_Records++;
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fwrite(&BufMeta, sizeof(bloc), 1, Xname);
            fseek(Xname, -sizeof(bloc), SEEK_CUR);
            fwrite(&BufFileData, sizeof(bloc), 1, Xname);
        }
        else
        {
            new_block = find_free_block(Xname);
            if (new_block == -1)
            {
                printf("Error: No free blocks available.\n");
                return;
            }
            BufFileData.next_bloc = new_block;
            fseek(Xname, new_block * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
            BufFileData.tenrg[0] = record;
            BufFileData.nbP++;
            BufFileData.next_bloc = -1;
            fseek(Xname, new_block * sizeof(bloc), SEEK_SET);
            fwrite(&BufFileData, sizeof(bloc), 1, Xname);
            BufMeta.MetaData.FileSize_Records++;
            BufMeta.MetaData.FileSize_Blocks++;
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fwrite(&BufMeta, sizeof(bloc), 1, Xname);
        }
    }
}

void search_record(FILE *Xname, int record_id, int *BlocNbr, int *DeplacementNbr)
{
    Tbloc BufMeta, BufFileData;
    int size, i, j;
    int current_block;

    fseek(Xname, sizeof(bloc), SEEK_SET);
    fread(&BufMeta, sizeof(bloc), 1, Xname);
    size = sizeof(BufMeta.allocation_table) / sizeof(BufMeta.allocation_table[0]);
    for (i = 1; i < size; i++)
    {
        if (BufMeta.allocation_table[i].etatbloc == 0)
        {
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fread(&BufMeta, sizeof(bloc), 1, Xname);
            current_block = BufMeta.MetaData.FirstBlock_Address;
            fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
            while (BufFileData.next_bloc != -1)
            {
                j = 0;
                while (j < BufFileData.nbP)
                {
                    if (BufFileData.tenrg[j].id == record_id)
                    {
                        *BlocNbr = current_block;
                        *DeplacementNbr = j;
                        return;
                    }
                    j++;
                }
                current_block = BufFileData.next_bloc;
                fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
                fread(&BufFileData, sizeof(bloc), 1, Xname);
            }
        }
    }
    printf("The record you are looking for does not exist\n");
}

void logical_deletion(FILE *Xname, int record_id)
{
    bloc BufFileData;
    int BlocNbr = -1, DeplacementNbr = -1;

    search_record(Xname, record_id, &BlocNbr, &DeplacementNbr);

    fseek(Xname, BlocNbr * sizeof(bloc), SEEK_SET);
    fread(&BufFileData, sizeof(bloc), 1, Xname);
    BufFileData.tenrg[DeplacementNbr].suppr = 1;
    fseek(Xname, -sizeof(bloc), SEEK_CUR);
    fwrite(&BufFileData, sizeof(bloc), 1, Xname);
}

void physical_deletion(FILE *Xname)
{
    Tbloc BufMeta, BufFileData;
    int size, i, j;
    int current_block;
    fseek(Xname, sizeof(bloc), SEEK_SET);
    fread(&BufMeta, sizeof(bloc), 1, Xname);
    size = sizeof(BufMeta.allocation_table) / sizeof(BufMeta.allocation_table[0]);
    for (i = 1; i < size; i++)
    {
        if (BufMeta.allocation_table[i].etatbloc == 0)
        {
            fseek(Xname, i * sizeof(bloc), SEEK_SET);
            fread(&BufMeta, sizeof(bloc), 1, Xname);
            current_block = BufMeta.MetaData.FirstBlock_Address;
            fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
            fread(&BufFileData, sizeof(bloc), 1, Xname);
            while (BufFileData.next_bloc != -1)
            {
                j = 0;
                while (j < BufFileData.nbP - 1)
                {
                    if (BufFileData.tenrg[j].suppr == 1)
                    {
                        BufFileData.tenrg[j] = BufFileData.tenrg[j + 1];
                        BufMeta.MetaData.FileSize_Records--;
                    }
                    else
                    {
                        j++;
                    }
                }
                fseek(Xname, -sizeof(bloc), SEEK_CUR);
                fwrite(&BufFileData, sizeof(bloc), 1, Xname);

                current_block = BufFileData.next_bloc;
                fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
                fread(&BufFileData, sizeof(bloc), 1, Xname);

                fseek(Xname, i * sizeof(bloc), SEEK_SET);
                fwrite(&BufMeta, sizeof(bloc), 1, Xname);
            }
        }
    }
}

void defragmentation(FILE *Xname, char fileName[])
{
    Tbloc Buf, BufCurrentBlock, BufNextBlock;
    int current_block, next_block;
    int i, size;

    size = sizeof(Buf.allocation_table) / sizeof(Buf.allocation_table[0]);
    fseek(Xname, sizeof(Tbloc), SEEK_SET);
    // Find the file metadata
    for (i = 0; i < size; i++)
    {
        if (Buf.allocation_table[i].etatbloc == 0)
        {
            fseek(Xname, i * sizeof(Tbloc), SEEK_SET);
            fread(&Buf, sizeof(Tbloc), 1, Xname);
            if (strcmp(Buf.MetaData.FileName, fileName) == 0)
            {
                break;
            }
        }
    }

    current_block = Buf.MetaData.FirstBlock_Address;

    while (current_block != -1)
    {
        fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
        fread(&BufCurrentBlock, sizeof(bloc), 1, Xname);

        // Shift valid records to fill gaps in the current block
        int valid = BufCurrentBlock.nbP;
        i = 0;
        while (i < BufCurrentBlock.nbP - 1)
        {
            if (BufCurrentBlock.tenrg[i].suppr == 1)
            {
                BufCurrentBlock.tenrg[i] = BufCurrentBlock.tenrg[i + 1];
                valid--;
            }
            else
            {
                i++;
            }
        }
        BufCurrentBlock.nbP = valid; // Update the count of valid records

        // Fill remaining space in the current block from the next block(s)
        next_block = BufCurrentBlock.next_bloc;
        fseek(Xname, next_block * sizeof(bloc), SEEK_SET);
        fread(&BufCurrentBlock, sizeof(bloc), 1, Xname);

        while (next_block != -1 && BufCurrentBlock.nbP < FB)
        {
            fseek(Xname, next_block * sizeof(bloc), SEEK_SET);
            fread(&BufNextBlock, sizeof(bloc), 1, Xname);

            // Move valid records from the next block to the current block
            for (i = 0; i < BufNextBlock.nbP && BufCurrentBlock.nbP < FB; i++)
            {
                if (BufNextBlock.tenrg[i].suppr == 0)
                {
                    BufCurrentBlock.tenrg[BufCurrentBlock.nbP++] = BufNextBlock.tenrg[i];
                    BufNextBlock.tenrg[i].suppr = 1; // Mark as logically deleted in the source block
                }
            }

            // Write the updated next block back to the file
            fseek(Xname, next_block * sizeof(bloc), SEEK_SET);
            fwrite(&BufNextBlock, sizeof(bloc), 1, Xname);
        }

        // Write the updated current block back to the file
        fseek(Xname, current_block * sizeof(bloc), SEEK_SET);
        fwrite(&BufCurrentBlock, sizeof(bloc), 1, Xname);

        // Move to the next block
        current_block = next_block;
    }

    printf("Defragmentation completed. All empty spaces removed and order maintained.\n");
}

// Tableau Non ordonnée
FILE *creerFichier(Metadata *meta)
{
    // Demander à l'utilisateur les informations du fichier
    printf("Entrez le nom du fichier : ");
    scanf("%s", meta->filename);
    // Demander le nombre d'enregistrements (utilisé pour calculer la taille des blocs)
    printf("Entrez le nombre d'enregistrements dans le fichier : ");
    int nbEnreg;
    scanf("%d", &nbEnreg);
    // Demander les modes d'organisation
    printf("Entrez le mode global (0: contigu, 1: chaîné) : ");
    scanf("%d", &meta->global_mode);
    printf("Entrez le mode interne (0: trié, 1: non trié) : ");
    scanf("%d", &meta->internal_mode);
    // Calculer la taille d'un bloc (par exemple, en fonction du nombre d'enregistrements)
    meta->block_size = FB * sizeof(TProduit); // Taille du bloc = nombre d'enregistrements * taille d'un produit
    meta->record_size = sizeof(TProduit);     // Taille d'un enregistrement
    // Initialiser le premier bloc disponible
    meta->first_block = 0;
    // Ouvrir le fichier en mode écriture binaire
    FILE *F;
    Ouvrir(&F, meta->filename, "wb");
    // Écrire les entêtes dans le fichier
    fwrite(meta, sizeof(Metadata), 1, F);
    printf("Entêtes écrites dans le fichier.\n");
    // Fermer le fichier
    Fermer(F);
    return F;
}

// Function for searching an element in an unordered list
recherche RecherchelementNO(int id, recherche *element, FILE *F, Metadata *meta)
{
    int bi, bs;
    bool Trouv = false;
    TBloc buffer;

    bs = LireEntete(meta, 5); // Last block
    bi = 0;                   // First block

    // Search through all blocks
    while (bi <= bs && !Trouv)
    {
        element->numeroBloc = bi;                  // Current block
        LireBloc(F, element->numeroBloc, &buffer); // Read the block into buffer

        // Search through all records in the block (linear search)
        for (int i = 0; i < buffer.ne; i++)
        {
            if (buffer.t[i].id == id)
            {
                element->deplacement = i; // Found the record
                Trouv = true;
                element->trouve = true;
                break;
            }
        }

        // Move to the next block if not found
        bi++;
    }

    // If the element was not found
    if (!Trouv)
    {
        element->numeroBloc = bi;
        element->deplacement = 0;
        element->trouve = false;
    }
    return *element;
}

// Fonction pour insérer un produit (Non ordonnée)
void insererNO(TProduit p, const char *nomFichier, Metadata *meta)
{
    recherche element;
    FILE *F;
    Buffer buffer;
    Ouvrir(&F, nomFichier, "r+");
    element = RecherchelementNO(p.id, &element, F, meta);
    TProduit y;
    int k;
    if (element.trouve == false)
    {
        bool suit = true;
        while (suit == true && element.numeroBloc <= LireEntete(meta, 5))
        {
            LireBloc(F, element.numeroBloc, &buffer.bloc_1);

            // Insert at the first available position (no shifting)
            if (buffer.bloc_1.ne < FB)
            {
                buffer.bloc_1.t[buffer.bloc_1.ne] = p;
                buffer.bloc_1.ne++;
                suit = false; // End the loop if inserted
            }
            else
            {
                // Block is full, write the current block and move to the next one
                EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
                element.numeroBloc++;
                element.deplacement = 0;
            }
        }
        // If the block is beyond the last known block, allocate a new block
        if (element.numeroBloc > LireEntete(meta, 5))
        {
            LireBloc(F, element.numeroBloc, &buffer.bloc_1);
            buffer.bloc_1.t[0] = p;
            buffer.bloc_1.ne = 1;
            EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
            MAJEntete(meta, 5, element.numeroBloc);
        }
    }

    Fermer(F);
}

int rename(const char *old_filename, const char *new_filename); // Retourne 0 si le fichier a été renommé avec succès sinon retourne -1
int remove(const char *filename);                               // Retourne 0 si le fichier a été supprimé avec succès sionon retourne -1

// Tableau ordonnée
//  Fonction de recherche
recherche RecherchelementO(int id, recherche *element, FILE *F, Metadata *meta)
{
    int bi, bs, inf, sup;
    bool Trouv, stop;
    TBloc buffer;

    bs = LireEntete(meta, 5); // Dernier bloc
    bi = 0;                   // Premier bloc
    Trouv = false;
    stop = false;

    while (bi <= bs && !Trouv && !stop)
    {
        element->numeroBloc = (bi + bs) / 2;       // Bloc du milieu
        LireBloc(F, element->numeroBloc, &buffer); // Lire le bloc dans buffer

        if (id >= buffer.t[0].id && id <= buffer.t[buffer.ne - 1].id)
        {
            inf = 0;
            sup = buffer.ne - 1;

            while (inf <= sup && !Trouv)
            {
                element->deplacement = (inf + sup) / 2;

                if (id == buffer.t[element->deplacement].id)
                {
                    Trouv = true;
                    element->trouve = true;
                }
                else if (id < buffer.t[element->deplacement].id)
                {
                    sup = element->deplacement - 1;
                }
                else
                {
                    inf = element->deplacement + 1;
                }
            }

            stop = true;
        }
        else if (id < buffer.t[0].id)
        {
            bs = element->numeroBloc - 1;
        }
        else
        {
            bi = element->numeroBloc + 1;
        }
    }

    if (!Trouv)
    {
        element->numeroBloc = bi;
        element->deplacement = 0;
        element->trouve = false;
    }

    return *element;
}

Ms Chargerlefichier(FILE *F, const char *nomFichier, Metadata *meta)
{
    Ms memoire;
    Ouvrir(&F, nomFichier, "r");
    int bs = LireEntete(meta, 5);
    memoire.taille = bs;
    memoire.memoire = (TBloc *)malloc(memoire.taille * sizeof(TBloc));
    if (memoire.memoire == NULL)
    {
        printf("Erreur de mémoire\n");
        exit(1);
    }
    Fermer(F);
    return memoire;
}

// Fonction pour insérer un produit
void insererO(TProduit p, const char *nomFichier, Metadata *meta)
{
    recherche element;
    FILE *F;
    Buffer buffer;
    Ouvrir(&F, nomFichier, "r+");
    element = RecherchelementO(p.id, &element, F, meta);
    TProduit y;
    int k;
    if (element.trouve == false)
    {
        bool suit = true;
        while (suit == true && element.numeroBloc <= LireEntete(meta, 5))
        {
            LireBloc(F, element.numeroBloc, &buffer.bloc_1);
            y = buffer.bloc_1.t[buffer.bloc_1.ne - 1];
            k = buffer.bloc_1.ne - 1;

            while (k > element.deplacement)
            {
                buffer.bloc_1.t[k] = buffer.bloc_1.t[k - 1];
                k--;
            }

            buffer.bloc_1.t[element.deplacement] = p;

            if (buffer.bloc_1.ne < FB)
            {
                buffer.bloc_1.ne++;
                buffer.bloc_1.t[buffer.bloc_1.ne - 1] = y;
                suit = false;
            }
            else
            {
                EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
                element.numeroBloc++;
                element.deplacement = 0;
                p = y;
            }
        }

        if (element.numeroBloc > LireEntete(meta, 5))
        {
            LireBloc(F, element.numeroBloc, &buffer.bloc_1);
            buffer.bloc_1.t[0] = p;
            buffer.bloc_1.ne = 1;
            EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
            MAJEntete(meta, 5, element.numeroBloc);
        }
    }

    Fermer(F);
}

// Fonction pour la suppression logique
void supplogique(int id, const char *nomFichier, Metadata *meta)
{
    recherche element;
    FILE *F;
    Buffer buffer;
    Ouvrir(&F, nomFichier, "r+");
    element = RecherchelementO(id, &element, F, meta);
    if (element.trouve == false)
    {
        printf("L'élément n'existe pas\n");
    }
    else
    {
        LireBloc(F, element.numeroBloc, &buffer.bloc_1);
        buffer.bloc_1.t[element.deplacement].suppr = true;
        EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
    }
    Fermer(F);
}

// Fonction pour la suppression physique
void suppphysique(int id, const char *nomFichier, Metadata *meta)
{
    FILE *F;
    recherche element;
    Buffer buffer;
    Ouvrir(&F, nomFichier, "r+");
    element = RecherchelementO(id, &element, F, meta);

    if (element.trouve == false)
    {
        printf("L'élément n'existe pas\n");
    }
    else
    {
        LireBloc(F, element.numeroBloc, &buffer.bloc_1);
        int k = buffer.bloc_1.ne - 1;

        while (element.deplacement < k)
        {
            buffer.bloc_1.t[element.deplacement] = buffer.bloc_1.t[element.deplacement + 1];
            element.deplacement++;
        }

        buffer.bloc_1.ne--;
        EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);

        if (buffer.bloc_1.ne == 0)
        {
            while (element.numeroBloc < LireEntete(meta, 5))
            {
                LireBloc(F, element.numeroBloc + 1, &buffer.bloc_1);
                EcrireBloc(F, element.numeroBloc, &buffer.bloc_1);
                element.numeroBloc++;
            }
        }
    }

    Fermer(F);
}

int lire_chaine(char *buffer, int max_length, const char *prompt);

int rechercher_element(int mode, int id, char *file_name, Metadata *meta);
int inserer_element(void *produit, char *file_name, Metadata *meta);
int supprimer_element(int mode, int id, char *file_name, Metadata *meta);

// Function Definitions
int lire_chaine(char *buffer, int max_length, const char *prompt) {
    printf("%s", prompt);
    if (fgets(buffer, max_length, stdin) == NULL) {
        return 0; // Input failed
    }
    buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
    return 1;
}


int rechercher_element(int mode, int id, char *file_name, Metadata *meta) {
    printf("Searching for element %d in file '%s' (mode: %d).\n", id, file_name, mode);
    return 1; // Simulated success
}

int inserer_element(void *produit, char *file_name, Metadata *meta) {
    printf("Inserting product into file '%s'.\n", file_name);
    meta->record_size++; // Simulate adding a record
    return 1; // Success
}

int supprimer_element(int mode, int id, char *file_name, Metadata *meta) {
    printf("Deleting element %d from file '%s' (mode: %d).\n", id, file_name, mode);
    if (meta->record_size > 0) {
        meta->record_size--; // Simulate record deletion
    }
    return 1; // Success
}


void afficher_menu()
{
    printf("\nMenu principal :\n");
    printf("1. Initialiser la memoire secondaire\n");
    printf("2. Creer un fichier\n");
    printf("3. Afficher l'etat de la memoire secondaire\n");
    printf("4. Afficher les métadonnees des fichiers\n");
    printf("5. Rechercher un enregistrement par ID\n");
    printf("6. Inserer un nouvel enregistrement\n");
    printf("7. Supprimer un enregistrement\n");
    printf("8. Defragmenter un fichier\n");
    printf("9. Supprimer un fichier\n");
    printf("10. Renommer un fichier\n");
    printf("11. Compactage de la memoire secondaire\n");
    printf("12. Vider la memoire secondaire\n");
    printf("13. Quitter le programme\n");
};

// System state structure
typedef struct
{
    bool is_initialized;
    int current_mode;
    int nb_blocks;
    int block_size;
} SystemState;

int main() {
    SystemState system = {false, 0, 0, 0}; // Track system initialization
    Metadata metadata_array[MAX_METADATA];
    int metadata_count = 0;

    int choice;
    do {
        afficher_menu();
        printf("Entrez votre choix : ");
        scanf("%d", &choice);
        nettoyer_buffer(); // Clear input buffer

        switch (choice) {
        case 1: {
            int mode, nb_blocks, block_size;
            printf("Mode (0: Contigu, 1: Chaîné): ");
            scanf("%d", &mode);
            printf("Nombre de blocs : ");
            scanf("%d", &nb_blocks);
            printf("Taille des blocs : ");
            scanf("%d", &block_size);
            system.is_initialized = initialiser_systeme(mode, nb_blocks, block_size);
            if (system.is_initialized) {
                system.current_mode = mode;
                system.nb_blocks = nb_blocks;
                system.block_size = block_size;
            }
            break;
        }
        case 2: {
            Metadata meta;
            FILE *file = creerFichier(&meta);
            if (file) {
                metadata_array[metadata_count++] = meta;
                fclose(file);
            }
            break;
        }
        case 3:
            afficher_etat_memoire(metadata_array, system.nb_blocks, system.block_size);
            break;
        case 4:
            display_metadata();
            break;
        case 5: {
            int id;
            char filename[FILENAME_MAX_LENGTH];
            printf("Nom du fichier : ");
            scanf("%s", filename);
            printf("ID de l'enregistrement à rechercher : ");
            scanf("%d", &id);
            int result = rechercher_element(system.current_mode, id, filename, metadata_array);
            if (result == 1) {
                printf("Enregistrement trouvé.\n");
            } else {
                printf("Enregistrement introuvable.\n");
            }
            break;
        }
        case 6: {
            TProduit produit;
            char filename[FILENAME_MAX_LENGTH];
            printf("Nom du fichier : ");
            scanf("%s", filename);
            printf("ID du produit : ");
            scanf("%d", &produit.id);
            printf("Nom du produit : ");
            scanf("%s", produit.nom);
            printf("Prix du produit : ");
            scanf("%f", &produit.prix);
            produit.suppr = 0; // Not deleted
            int result = inserer_element(&produit, filename, metadata_array);
            if (result == 1) {
                printf("Enregistrement inséré avec succès.\n");
            } else {
                printf("Erreur lors de l'insertion.\n");
            }
            break;
        }
        case 7: {
            int id;
            char filename[FILENAME_MAX_LENGTH];
            int mode;
            printf("Nom du fichier : ");
            scanf("%s", filename);
            printf("Mode de suppression (0: Logique, 1: Physique): ");
            scanf("%d", &mode);
            printf("ID de l'enregistrement à supprimer : ");
            scanf("%d", &id);
            int result = supprimer_element(mode, id, filename, metadata_array);
            if (result == 1) {
                printf("Enregistrement supprimé avec succès.\n");
            } else {
                printf("Erreur lors de la suppression.\n");
            }
            break;
        }
        case 8: {
            char filename[FILENAME_MAX_LENGTH];
            printf("Nom du fichier : ");
            scanf("%s", filename);
            defragmentation(NULL, filename); // Implement file pointer logic as needed
            break;
        }
        case 9: {
            char filename[FILENAME_MAX_LENGTH];
            printf("Nom du fichier à supprimer : ");
            scanf("%s", filename);

            if (remove(filename) == 0) {
               printf("Fichier '%s' supprimé avec succès.\n", filename);
            } else {
             perror("Erreur lors de la suppression du fichier");
             }
            break;
        }
        case 10: {
            char old_filename[FILENAME_MAX_LENGTH];
            char new_filename[FILENAME_MAX_LENGTH];   
            printf("Nom actuel du fichier : ");
            scanf("%s", old_filename);
            printf("Nouveau nom du fichier : ");
            scanf("%s", new_filename);
    
            if (rename(old_filename, new_filename) == 0) {
                printf("Fichier renommé avec succès en '%s'.\n", new_filename);
                } else {
                perror("Erreur lors du renommage du fichier");
            }
            break;
        }
        case 11:
            for (int i = 0; i < metadata_count; i++) {
                FILE *file = fopen(metadata_array[i].filename, "rb+");
                if (!file) {
                    perror("Erreur d'ouverture du fichier");
                    continue;
                }
                Compactage(file, &metadata_array[i]);
                fclose(file);
            }
            printf("Compactage de la mémoire secondaire terminé.\n");
            break;
        case 12: {
            char confirmation;   
            printf("Voulez-vous vraiment effacer toute la mémoire secondaire ? (y/n) : ");
            scanf(" %c", &confirmation);
            if (confirmation == 'y' || confirmation == 'Y') {
            for (int i = 0; i < metadata_count; i++) {
                if (remove(metadata_array[i].filename) == 0) {
                    printf("Fichier '%s' supprimé.\n", metadata_array[i].filename);
                } else {
                perror("Erreur lors de la suppression du fichier");
                }
            }
            metadata_count = 0;
            system.is_initialized = false;
            printf("Mémoire secondaire vidée avec succès.\n");
            } else {
                printf("Opération annulée.\n");
            }
            break;
        }
        case 13:
            printf("Au revoir!\n");
            break;
        default:
            printf("Choix invalide. Veuillez réessayer.\n");
        }
    } while (choice != 13);

    return 0;
}
