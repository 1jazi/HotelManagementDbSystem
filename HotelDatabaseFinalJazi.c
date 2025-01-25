#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#define HashSize 100
// indexing by id and string , hashing
typedef struct
{
    void *data;
    void *next;
} Node;

typedef struct HashNode
{
    void *data;
    struct HashNode *next;
} HashNode;

typedef struct
{
    HashNode *buckets[HashSize];
} HashTable;

int hashFunction(int key)
{
    return key % HashSize;
}

typedef struct
{
    const char *name;
    Node *head;
    HashTable idHashTable;
    HashTable nameHashTable;
    // pthread_mutex_t mutex;
    int lockCounter;
    const char *filename;
    size_t dataSize;
    void (*displayFunction)(void *);
    void (*inputFunction)(void *);
    int (*idExtract)(void *);
    const char *(*nameExtract)(void *);
} Table;

void lockTable(Table *table)
{
    while (true)
    {
        if (table->lockCounter == 0)
        {
            table->lockCounter = 1;
            break;
        }
        printf("Table %s is locked. Waiting...\n", table->name);
        usleep(100);
    }
    printf("Table %s is now locked by thread %ld\n", table->name, (long)pthread_self());
}

void unlockTable(Table *table)
{
    table->lockCounter = 0;
    printf("Table %s is now unlocked by thread %ld\n", table->name, (long)pthread_self());
}

int stringHashFunction(const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HashSize;
}

struct Customer
{
    int customerID;
    char name[100];
    char email[100];
    char phone[20];
    char address[200];
};

struct Room
{
    int roomID;
    char roomType[50];
    double price;
    int availability;
};

struct Reservation
{
    int reservationID;
    char checkInDate[20];
    char checkOutDate[20];
    int customerID;
    int roomID;
};

struct Amenity
{
    int RoomID;
    int AmenityID;
};

struct Amenity_Type
{
    int AmenityID;
    char AmenityName[100];
};

struct CUTSOMER_PLACES_ROOM
{
    int customerID;
    int RoomID;
    char phone[20];
};

//  tables
Table tables[6];

void displayCustomer(void *data);
void displayRoom(void *data);
void displayReservation(void *data);
void displayAmenity(void *data);
void displayAmenityType(void *data);
void displayCustomerPlacesRoom(void *data);
// void inputCustomer(void *data);

void *backupTable(void *arg)
{
    Table *table = (Table *)arg;
    printf("\nBackup thread started for %s\n", table->name);
    lockTable(table);

    FILE *file = fopen(table->filename, "wb");
    if (file)
    {
        Node *current = table->head;
        while (current)
        {
            fwrite(current->data, table->dataSize, 1, file);
            current = current->next;
        }
        fclose(file);
        printf("Backup completed for %s\n", table->name);
    }

    unlockTable(table);
    return NULL;
}

void *findById(Table *table, int id)
{
    int hash_index = hashFunction(id);
    HashNode *current = table->idHashTable.buckets[hash_index];

    while (current)
    {
        if (table->idExtract(current->data) == id)
        {
            return current->data;
        }
        current = current->next;
    }
    return NULL;
}

void display(Table *table)
{
    lockTable(table);

    printf("\n%s List:\n", table->name);
    Node *current = table->head;

    if (!current)
    {
        printf("No records found!\n");
    }
    else
    {
        while (current)
        {
            table->displayFunction(current->data);
            current = current->next;
        }
    }

    unlockTable(table);
}

void insert(Table *table)
{
    lockTable(table);

    void *newData = malloc(table->dataSize);
    if (!newData)
    {
        printf("Memory allocation failed!\n");
        unlockTable(table);
        return;
    }

    table->inputFunction(newData);

    int id = table->idExtract(newData);

    void *data = findById(table, id);
    if (data)
    {
        printf("Record with ID %d already exists! Retry again.\n", id);
        free(newData);
        unlockTable(table);
        return;
    }

    // insert in linkedlist
    Node *newNode = malloc(sizeof(Node));
    if (!newNode)
    {
        printf("Memory allocation failed!\n");
        free(newData);
        unlockTable(table);
        return;
    }
    newNode->data = newData;
    newNode->next = table->head;
    table->head = newNode;

    // insert id in hash table
    int idHashIndex = hashFunction(id);
    HashNode *idHashNode = malloc(sizeof(HashNode));
    if (!idHashNode)
    {
        printf("Memory allocation failed!\n");
        free(newData);
        free(newNode);
        unlockTable(table);
        return;
    }
    idHashNode->data = newData;
    idHashNode->next = table->idHashTable.buckets[idHashIndex];
    table->idHashTable.buckets[idHashIndex] = idHashNode;

    // condition for name exist and insert it in hash table
    if (table->nameExtract)
    {
        const char *name = table->nameExtract(newData);
        int nameHashIndex = stringHashFunction(name);
        HashNode *nameHashNode = malloc(sizeof(HashNode));
        if (!nameHashNode)
        {
            printf("Memory allocation failed!\n");
            free(newData);
            free(newNode);
            free(idHashNode);
            unlockTable(table);
            return;
        }
        nameHashNode->data = newData;
        nameHashNode->next = table->nameHashTable.buckets[nameHashIndex];
        table->nameHashTable.buckets[nameHashIndex] = nameHashNode;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, backupTable, table);
    pthread_detach(thread);

    unlockTable(table);
    printf("Record added successfully!\n");
}

void update(Table *table, int id)
{
    lockTable(table);

    void *data = findById(table, id);
    if (data)
    {

        int old_id = table->idExtract(data);
        int old_hash_index = hashFunction(old_id);
        HashNode **prev_ptr = &table->idHashTable.buckets[old_hash_index];
        HashNode *current = *prev_ptr;

        while (current)
        {
            if (table->idExtract(current->data) == old_id)
            {
                *prev_ptr = current->next;
                free(current);
                break;
            }
            prev_ptr = &current->next;
            current = current->next;
        }

        table->inputFunction(data);

        int new_id = table->idExtract(data);
        int new_hash_index = hashFunction(new_id);
        HashNode *newNode = malloc(sizeof(HashNode));
        if (!newNode)
        {
            printf("Memory allocation failed!\n");
            unlockTable(table);
            return;
        }
        newNode->data = data;
        newNode->next = table->idHashTable.buckets[new_hash_index];
        table->idHashTable.buckets[new_hash_index] = newNode;

        pthread_t thread;
        pthread_create(&thread, NULL, backupTable, table);
        pthread_detach(thread);

        printf("Record updated successfully!\n");
    }
    else
    {
        printf("Record not found!\n");
    }

    unlockTable(table);
}

void delete(Table *table, int id)
{
    lockTable(table);

    int idHashIndex = hashFunction(id);
    HashNode *id_current = table->idHashTable.buckets[idHashIndex];
    HashNode *id_prev = NULL;

    Node *list_current = table->head;
    Node *list_prev = NULL;

    while (list_current)
    {
        int current_id = table->idExtract(list_current->data);

        if (current_id == id)
        {
            // remove id hash table
            while (id_current)
            {
                if (table->idExtract(id_current->data) == id)
                {
                    if (id_prev)
                    {
                        id_prev->next = id_current->next;
                    }
                    else
                    {
                        table->idHashTable.buckets[idHashIndex] = id_current->next;
                    }
                    free(id_current);
                    break;
                }
                id_prev = id_current;
                id_current = id_current->next;
            }

            // remove from linked list
            if (list_prev)
            {
                list_prev->next = list_current->next;
            }
            else
            {
                table->head = list_current->next;
            }

            free(list_current->data);
            free(list_current);

            // rewrite the file
            FILE *file = fopen(table->filename, "wb");
            if (file)
            {
                Node *node = table->head;
                while (node)
                {
                    fwrite(node->data, table->dataSize, 1, file);
                    node = node->next;
                }
                fclose(file);
            }

            printf("Record deleted successfully!\n");
            unlockTable(table);
            return;
        }

        list_prev = list_current;
        list_current = list_current->next;
    }

    printf("Record not found!\n");
    unlockTable(table);
}

void *findByName(Table *table, const char *name)
{
    int hash_index = stringHashFunction(name);
    HashNode *current = table->nameHashTable.buckets[hash_index];

    while (current)
    {
        if (strcmp(table->nameExtract(current->data), name) == 0)
        {
            return current->data;
        }
        current = current->next;
    }
    return NULL;
}
const char *extractCustomerName(void *data)
{
    return ((struct Customer *)data)->name;
}
int extractCustomerId(void *data)
{
    return ((struct Customer *)data)->customerID;
}
const char *extractRoomType(void *data)
{
    return ((struct Room *)data)->roomType;
}

int extractRoomId(void *data)
{
    return ((struct Room *)data)->roomID;
}

int extractReservationId(void *data)
{
    return ((struct Reservation *)data)->reservationID;
}

int extractReservationCustomerId(void *data)
{
    return ((struct Reservation *)data)->customerID;
}

int extractReservationRoomId(void *data)
{
    return ((struct Reservation *)data)->roomID;
}

int extractAmenityRoomId(void *data)
{
    return ((struct Amenity *)data)->RoomID;
}

int extractAmenityAmenityId(void *data)
{
    return ((struct Amenity *)data)->AmenityID;
}

int extractAmenityTypeId(void *data)
{
    return ((struct Amenity_Type *)data)->AmenityID;
}

const char *extractAmenityTypeName(void *data)
{
    return ((struct Amenity_Type *)data)->AmenityName;
}

int extractCustomerPlacesRoomId(void *data)
{
    return ((struct CUTSOMER_PLACES_ROOM *)data)->RoomID;
}

int extractCustomerPlacesRoomCustomerId(void *data)
{
    return ((struct CUTSOMER_PLACES_ROOM *)data)->customerID;
}

void displayCustomer(void *data)
{
    struct Customer *customer = (struct Customer *)data;
    printf("ID: %d, Name: %s, Email: %s, Phone: %s, Address: %s\n",
           customer->customerID, customer->name, customer->email,
           customer->phone, customer->address);
}
void displayRoom(void *data)
{
    struct Room *room = (struct Room *)data;
    printf("Room ID: %d, Room Type: %s, Price: %.2lf, Availability: %s\n",
           room->roomID, room->roomType, room->price,
           room->availability ? "Available" : "Not Available");
}

void displayReservation(void *data)
{
    struct Reservation *reservation = (struct Reservation *)data;
    printf("Reservation ID: %d, Check-in: %s, Check-out: %s, Customer ID: %d, Room ID: %d\n",
           reservation->reservationID, reservation->checkInDate, reservation->checkOutDate,
           reservation->customerID, reservation->roomID);
}

void displayAmenity(void *data)
{
    struct Amenity *amenity = (struct Amenity *)data;
    printf("Room ID: %d, Amenity ID: %d\n", amenity->RoomID, amenity->AmenityID);
}

void displayAmenityType(void *data)
{
    struct Amenity_Type *amenityType = (struct Amenity_Type *)data;
    printf("Amenity ID: %d, Amenity Name: %s\n", amenityType->AmenityID, amenityType->AmenityName);
}

void displayCustomerPlacesRoom(void *data)
{
    struct CUTSOMER_PLACES_ROOM *cpr = (struct CUTSOMER_PLACES_ROOM *)data;
    printf("Customer ID: %d, Room ID: %d, Phone: %s\n", cpr->customerID, cpr->RoomID, cpr->phone);
}

void inputCustomer(void *data)
{
    struct Customer *customer = (struct Customer *)data;
    printf("Enter Customer ID: ");
    scanf("%d", &customer->customerID);
    printf("Enter Name: ");
    scanf(" %[^\n]s", customer->name);
    printf("Enter Email: ");
    scanf(" %[^\n]s", customer->email);
    printf("Enter Phone: ");
    scanf(" %[^\n]s", customer->phone);
    printf("Enter Address: ");
    scanf(" %[^\n]s", customer->address);
}
void inputRoom(void *data)
{
    struct Room *room = (struct Room *)data;
    printf("Enter Room ID: ");
    scanf("%d", &room->roomID);
    printf("Enter Room Type: ");
    scanf(" %[^\n]s", room->roomType);
    printf("Enter Room Price: ");
    scanf("%lf", &room->price);
    printf("Enter Availability (1 for available, 0 for unavailable): ");
    scanf("%d", &room->availability);
}

void inputReservation(void *data)
{
    struct Reservation *reservation = (struct Reservation *)data;
    printf("Enter Reservation ID: ");
    scanf("%d", &reservation->reservationID);
    printf("Enter Check-in Date (YYYY-MM-DD): ");
    scanf(" %[^\n]s", reservation->checkInDate);
    printf("Enter Check-out Date (YYYY-MM-DD): ");
    scanf(" %[^\n]s", reservation->checkOutDate);
    printf("Enter Customer ID: ");
    scanf("%d", &reservation->customerID);
    printf("Enter Room ID: ");
    scanf("%d", &reservation->roomID);
}

void inputAmenity(void *data)
{
    struct Amenity *amenity = (struct Amenity *)data;
    printf("Enter Room ID: ");
    scanf("%d", &amenity->RoomID);
    printf("Enter Amenity ID: ");
    scanf("%d", &amenity->AmenityID);
}

void inputAmenityType(void *data)
{
    struct Amenity_Type *amenityType = (struct Amenity_Type *)data;
    printf("Enter Amenity ID: ");
    scanf("%d", &amenityType->AmenityID);
    printf("Enter Amenity Name: ");
    scanf(" %[^\n]s", amenityType->AmenityName);
}

void inputCustomerPlacesRoom(void *data)
{
    struct CUTSOMER_PLACES_ROOM *cpr = (struct CUTSOMER_PLACES_ROOM *)data;
    printf("Enter Customer ID: ");
    scanf("%d", &cpr->customerID);
    printf("Enter Room ID: ");
    scanf("%d", &cpr->RoomID);
    printf("Enter Phone Number: ");
    scanf(" %[^\n]s", cpr->phone);
}

void menuCallFunction(Table *table)
{
    int choice;
    do
    {
        printf("\n%s Operations:\n", table->name);
        printf("1. Insert\n");
        printf("2. Display\n");
        printf("3. Update\n");
        printf("4. Delete\n");
        printf("5. Search\n");
        printf("6. Return to Main Menu\n");
        printf("Enter choice: \n");
        scanf("%d", &choice);

        int id;
        switch (choice)
        {
        case 1:
            insert(table);
            break;
        case 2:
            display(table);
            break;
        case 3:
            printf("Enter ID to update: ");
            scanf("%d", &id);
            update(table, id);
            break;
        case 4:
            printf("Enter ID to delete: ");
            scanf("%d", &id);
            delete (table, id);
            break;
        case 5:
            // lockTable(table);

            printf("You want the search by id or name\n");
            printf("1- Id\n");
            printf("2- Name\n");
            int searchOption = 0;
            scanf("%d", &searchOption);

            if (searchOption == 1)
            {

                printf("Enter ID to search: ");
                scanf("%d", &id);
                printf("Searching by id in %s table....\n", table->name);
                void *data = findById(table, id);

                if (data)
                {
                    // printf("data");
                    if (strcmp(table->name, "Customer") == 0)
                    {
                        displayCustomer(data);
                    }
                    else if (strcmp(table->name, "Room") == 0)
                    {

                        displayRoom(data);
                    }
                    else if (strcmp(table->name, "Amenity") == 0)
                    {
                        displayAmenity(data);
                    }
                    else if (strcmp(table->name, "Reservation") == 0)
                    {
                        printf("gggg");
                        displayRoom(data);
                    }
                    else if (strcmp(table->name, "Amenity_Type") == 0)
                    {
                        displayAmenity(data);
                    }
                    else if (strcmp(table->name, "CUTSOMER_PLACES_ROOM") == 0)
                    {
                        displayAmenity(data);
                    }
                    //  struct Customer* customer = (struct Customer*)data;
                    //   printf("  ->  ID: %d, Name: %s, Email: %s, Phone: %s, Address: %s\n",
                    //   customer->customerID, customer->name, customer->email,
                    //   customer->phone, customer->address);
                }
                else
                {
                    printf("Record not found");
                }
            }
            else if (searchOption == 2)
            {
                printf("Enter Name to search: ");
                printf("Enter Name: ");
                char name[100];
                scanf(" %[^\n]s", name);
                printf("Searching by name in %s table....\n", table->name);
                void *data = findByName(table, name);

                if (data)
                {
                    // printf("data");
                    if (strcmp(table->name, "Customer") == 0)
                    {
                        displayCustomer(data);
                    }
                    else if (strcmp(table->name, "Room") == 0)
                    {
                        printf("gggg");
                        displayRoom(data);
                    }
                    else if (strcmp(table->name, "Amenity") == 0)
                    {
                        displayAmenity(data);
                    }
                    //  struct Customer* customer = (struct Customer*)data;
                    //   printf("  ->  ID: %d, Name: %s, Email: %s, Phone: %s, Address: %s\n",
                    //   customer->customerID, customer->name, customer->email,
                    //   customer->phone, customer->address);
                }
                else
                {
                    printf("Record not found");
                }
            }
            else
            {
                printf("Invallid choice");
            }

        case 6:
            return;
        default:
            printf("Invalid choice!\n");
        }
    } while (choice != 5);
}

// Initialize tables

void initializeTables()
{
    // Initialize Customer table
    tables[0] = (Table){
        .name = "Customer",
        .head = NULL,
        .filename = "customers.dat",
        .dataSize = sizeof(struct Customer),
        .displayFunction = displayCustomer,
        .inputFunction = inputCustomer,
        .idExtract = extractCustomerId,
        .nameExtract = extractCustomerName,
        .lockCounter = 0,

    };
    // pthread_mutex_init(&tables[0].mutex, NULL);

    tables[1] = (Table){
        .name = "Room",
        .head = NULL,
        .filename = "room.dat",
        .dataSize = sizeof(struct Room),
        .displayFunction = displayRoom,
        .inputFunction = inputRoom,
        .idExtract = extractRoomId,
        .nameExtract = extractRoomType,
        .lockCounter = 0,
    };
    // pthread_mutex_init(&tables[1].mutex, NULL);

    tables[2] = (Table){
        .name = "Reservation",
        .head = NULL,
        .filename = "reservation.dat",
        .dataSize = sizeof(struct Reservation),
        .displayFunction = displayReservation,
        .inputFunction = inputReservation,
        .idExtract = extractReservationId,
        .nameExtract = NULL,
        .lockCounter = 0,

    };
    // pthread_mutex_init(&tables[2].mutex, NULL);

    tables[3] = (Table){
        .name = "Amenity",
        .head = NULL,
        .filename = "amenity.dat",
        .dataSize = sizeof(struct Amenity),
        .displayFunction = displayAmenity,
        .inputFunction = inputAmenity,
        .idExtract = extractAmenityRoomId,
        .nameExtract = NULL,
        .lockCounter = 0,

    };
    // pthread_mutex_init(&tables[3].mutex, NULL);

    tables[4] = (Table){
        .name = "Amenity_Type",
        .head = NULL,
        .filename = "amenity_type.dat",
        .dataSize = sizeof(struct Amenity_Type),
        .displayFunction = displayAmenityType,
        .inputFunction = inputAmenityType,
        .idExtract = extractAmenityTypeId,
        .nameExtract = extractAmenityTypeName,
        .lockCounter = 0,
    };
    // pthread_mutex_init(&tables[4].mutex, NULL);

    tables[5] = (Table){
        .name = "CUTSOMER_PLACES_ROOM",
        .head = NULL,
        .filename = "CUTSOMER_PLACES_ROOM.dat",
        .dataSize = sizeof(struct CUTSOMER_PLACES_ROOM),
        .displayFunction = displayCustomerPlacesRoom,
        .inputFunction = inputCustomerPlacesRoom,
        .idExtract = extractCustomerPlacesRoomId,
        .nameExtract = NULL,
        .lockCounter = 0,
    };
    // pthread_mutex_init(&tables[5].mutex, NULL);

    // retrieve data from files

    for (int i = 0; i < 6; i++)
    {
        FILE *file = fopen(tables[i].filename, "rb");
        if (file)
        {
            void *data = malloc(tables[i].dataSize);
            while (fread(data, tables[i].dataSize, 1, file) == 1)
            {
                Node *node = malloc(sizeof(Node));
                node->data = data;
                node->next = tables[i].head;
                tables[i].head = node;

                int id = tables[i].idExtract(data);
                int idHashIndex = hashFunction(id);
                HashNode *idHashNode = malloc(sizeof(HashNode));
                idHashNode->data = data;
                idHashNode->next = tables[i].idHashTable.buckets[idHashIndex];
                tables[i].idHashTable.buckets[idHashIndex] = idHashNode;

                if (tables[i].nameExtract)
                {
                    const char *name = tables[i].nameExtract(data);
                    int nameHashIndex = stringHashFunction(name);
                    HashNode *nameHashNode = malloc(sizeof(HashNode));
                    nameHashNode->data = data;
                    nameHashNode->next = tables[i].nameHashTable.buckets[nameHashIndex];
                    tables[i].nameHashTable.buckets[nameHashIndex] = nameHashNode;
                }

                data = malloc(tables[i].dataSize);
            }
            free(data);
            fclose(file);
        }
    }
}

void cleanup()
{
    for (int i = 0; i < 6; i++)
    {
        Node *current = tables[i].head;
        while (current)
        {
            Node *temp = current;
            current = current->next;
            free(temp->data);
            free(temp);
        }
        // pthread_mutex_destroy(&tables[i].mutex);
    }
}

int main()
{
    int choice;

    printf("\n\n\t\t*************************************************\n");
    printf("\t\t*                                               *\n");
    printf("\t\t*          -----------------------------        *\n");
    printf("\t\t*                 HOTEL MANAGEMENT              *\n");
    printf("\t\t*          -----------------------------        *\n");
    printf("\t\t*                                               *\n");
    printf("\t\t*               Database Programming            *\n");
    printf("\t\t*************************************************\n\n\n");

    initializeTables();

    do
    {
        printf("\nMain Menu:\n");
        printf("1. Customer Management\n");
        printf("2. Room Management\n");
        printf("3. Reservation Management\n");
        printf("5. Amenity type Management\n");
        printf("6. Customer Places Room Management\n");
        printf("7. Exit\n");
        printf("Enter choice: \n");
        scanf("%d", &choice);
        switch (choice)
        {
        case 1:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 2:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 3:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 4:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 5:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 6:
            menuCallFunction(&tables[choice - 1]);
            break;
        case 7:
            printf("Thank you for using Hotel Management System!\n");
            cleanup();
            break;
        default:
            printf("Invalid choice!\n");
        }
    } while (choice != 7);

    return 0;
}
