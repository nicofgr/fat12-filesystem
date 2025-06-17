#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FT1_START    0x0200
#define FT2_START    0x1400
#define ROOTDIR_ADDR 0x2600
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
FILE *DISK;

void getClusterFromAdrress(FILE *DISK, int address, unsigned char *ponteiro_cluster){
        fseek(DISK, address, SEEK_SET);
        fread(ponteiro_cluster, sizeof(char), 512, DISK);
}

unsigned char getByteFromAddress(FILE *DISK, int address){
        fseek(DISK, address, SEEK_SET);
        unsigned char *ponteiroByte = malloc(1);
        fread(ponteiroByte, sizeof(char), 1, DISK);
        char byteLido = *ponteiroByte;
        free(ponteiroByte);
        return byteLido;
}

//char getFile

unsigned int getRootAddress(FILE *DISK){
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, 0, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);
        char numFATs = *(char*)&cluster_ptr[0x10];
       //printf("Number of FATs: %d tables\n", numFATs);
        char sizeFAT = *(short*)&cluster_ptr[0x16];
        //printf("Size of FAT: %d blocks\n", sizeFAT);
        int rootAddress = ((numFATs*sizeFAT)+1)*512;
        free(cluster_ptr);
        return rootAddress;
}

void printDirectory(FILE *DISK, int address){
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, address, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);
        for(int i = 0; i < 3; i++){
                char* fileName = (char*)&cluster_ptr[0x00+(i*32)];
                if(*fileName == 0x00)
                        continue; 
                char* fileExt = (char*)&cluster_ptr[0x08+(i*32)];
                short fileAddress = *(short*)&cluster_ptr[0x1a+(i*32)];
                fileAddress = (fileAddress+31)*512;
                short fileSize = *(short*)&cluster_ptr[0x1a+(i*32)];
                //printf("%x\n", *fileName);
                printf("  ├─ %.8s.%.3s", fileName, fileExt);
                if(fileName[11] == 0x10)
                        printf("/"); 
                printf("   (0x%x)", fileAddress);
                printf("\n");
        }
}

void printRootFiles(FILE *DISK, int option){
        unsigned int rootAddress = getRootAddress(DISK);
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, rootAddress, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);

        //short rootSize = *(short*)&cluster_ptr[0x11];
        //printf("Root size: %d entries / %d bytes / %d blocks\n", rootSize, rootSize*32, rootSize*32/512); 
        //printf("Boot: 1 block / FATs: 18 blocks / Root: 3 blocks\n");


        for(int i = 0; i < 16; i++){
                char* fileName = (char*)&cluster_ptr[0x00+(i*32)];
                if(*fileName == 0x00)
                        continue; 
                char* fileExt = (char*)&cluster_ptr[0x08+(i*32)];
                u16 fileAddress = *(u16*)&cluster_ptr[0x1a+(i*32)];
                fileAddress = (fileAddress+31)*512;
                u32 fileSize = *(u32*)&cluster_ptr[0x1c+(i*32)];
                //printf("%x\n", *fileName);
                //printf("├─ %.8s.%.3s (Addr: 0x%x) (Size: 0x%.8x / %d bytes)\n", fileName, fileExt, fileAddress, fileSize, fileSize);
                printf("├─ %.8s.%.3s (Addr: 0x%x) (Size: %d bytes)\n", fileName, fileExt, fileAddress, fileSize);
                if((fileName[11] == 0x10) && (option == 1)){
                        printDirectory(DISK, fileAddress);
                }
        }
}

void printFile(int fileAddress, int fileSize){
        unsigned char *cluster_ptr = malloc(fileSize);
        fseek(DISK, fileAddress, SEEK_SET);
        fread(cluster_ptr, sizeof(char), fileSize, DISK);

        for(int i = 0; i < fileSize; i++){
                printf("%c", cluster_ptr[i]);
        }
}

void copyFileToSystem(int fileAddress, int fileSize){
        unsigned char *cluster_ptr = malloc(fileSize);
        fseek(DISK, fileAddress, SEEK_SET);
        fread(cluster_ptr, sizeof(char), fileSize, DISK);
        FILE* newFile;
        newFile = fopen("Test", "w");
        for(int i = 0; i < fileSize; i++){
                fprintf(newFile, "%c", cluster_ptr[i]);
        }
        fclose(newFile);
}




typedef struct{
        u32 nextCluster1;
        u32 nextCluster2;
}metaCluster;

typedef struct{
        char* items[3];
        u32     lenght;
        u32   capacity;
}clusterArray;

metaCluster getNormalizedMetaCluster(int index){
        /**
         * Para ler a tabela de arquivos é necessário pegar seções de
         * 3 em 3 bytes, inverter o byte do meio (40 -> 04) e juntar
         * com os bytes ao redor
         */
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, 512, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);
        char byte[3] = {};
        metaCluster metaClus;
        for(int i = 0; i < 3; i++){
                byte[i] = cluster_ptr[i+index*3];
                //printf("%.2x\n", cluster_ptr[i+index*3]);
        }
        //byte[0] = 0x12;
        //byte[1] = 0x50;
        //byte[2] = 0x34;
        metaClus.nextCluster1 = ((byte[1] & 0x00F)<<8) + (byte[0] & 0x0FF);
        metaClus.nextCluster2 = ((byte[2]<<4) & 0xFF0) + ((byte[1] >> 4) & 0xF);
        return metaClus;
        //printf("%.3x ", metaClus.nextCluster1);
        //printf("%.3x ", metaClus.nextCluster2);
}

u32 getNormalizedClusterByIndex(int index){
        metaCluster meta = getNormalizedMetaCluster(index/2);
        if(index%2)
                return meta.nextCluster2;
        return meta.nextCluster1;
}

int getFirstEmptyClusterIndex(){
        int i = 0;
        u32 cluster;
        while(1){
                cluster = getNormalizedClusterByIndex(i);
                if (cluster == 0x000)
                        break;
                i++;
        }
        //printf("%d\n", i);
        return i;
}

metaCluster getLastMegaCluster(){
        int emptyClusterIndex = getFirstEmptyClusterIndex(); 
        metaCluster lastMeta = getNormalizedMetaCluster(emptyClusterIndex/2);
        return lastMeta;
}


int getFileNumInFileTable(){
        int nFiles = 0;
        u32 cluster;
        int i = 0;
        while(1){
                cluster = getNormalizedClusterByIndex(i);
                if(cluster == 0x000)
                        break;
                if(cluster == 0xfff)
                        nFiles++;
                i++;
        }
        return nFiles;
}

void printUntilLastClusterFromFile(unsigned int index){
        u32 cluster = getNormalizedClusterByIndex(0);
        int i = 0;
        while(1){
                cluster = getNormalizedClusterByIndex(i);
                printf("%.3x ", cluster);
                if(cluster == 0xfff)
                        break;
                i++;
        }
}


void printHumanReadableFileTable(){
        int lastClusterIndex = getFirstEmptyClusterIndex();
        int nFiles = 0;
        for(int i = 0; i < lastClusterIndex; i++){
                //printf("File %d | Clusters :");
                u32 cluster = getNormalizedClusterByIndex(i);
                printf("%.3x ", cluster);
                if(cluster == 0xfff){
                        nFiles++;
                        printf("\n");
                }
        }
        printf("%d files found.\n", nFiles);

        puts("Format in file (normalized):");
        for(int i = 0; i < lastClusterIndex; i++){
                u32 cluster = getNormalizedClusterByIndex(i);
                printf("%.3x ", cluster);
        }
        printf("\n");
}

void readFileTable(){
        for(int i = 0; i < 3; i++)
                getNormalizedMetaCluster(i);
}

void printFileHex(){
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, ROOTDIR_ADDR, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);
        unsigned char *fileData = malloc(32);
        int ind = 2;
        for(int ind = 0; ind < 14; ind++){
                printf("Arquivo %d:\n", ind);
                fileData = &cluster_ptr[ind*32];
                if(fileData[ind] == 0x00){
                        printf("[LIVRE]\n");
                        continue;
                }
                printf("%.36s\n", fileData); 
                for(int i = 0; i < 32; i++){
                        printf("%.2x ", fileData[i]);
                        if(i == 15)
                                puts("");
                }
                puts("");
                puts("");
        }
}

u16 allocateFAT(){
        int right = 0;
        unsigned char byte[3] = {};

        // MODIFYING LAST CLUSTER
        metaCluster metaClus = getLastMegaCluster();
        if(metaClus.nextCluster1 == 0x000){
                metaClus.nextCluster1 = 0xfff;
        }else{
                metaClus.nextCluster2 = 0xfff;
                right = 1;
        }

        byte[0] = (metaClus.nextCluster1 & 0x0FF);
        byte[1] = ((metaClus.nextCluster1 >> 8) & 0x00F) + ((metaClus.nextCluster2 << 4) & 0x0F0) & 0xFF;
        byte[2] = (metaClus.nextCluster2>>4) & 0x0FF;

        // GETTING LAST CLUSTER ADDRESS
        int lastIndex = getFirstEmptyClusterIndex()/2;
        u16 entryNumber = right+lastIndex*2;
        metaClus = getNormalizedMetaCluster(lastIndex);
        int lastEntryAddress = FT1_START+(lastIndex*3);
        int lastEntryAddressCopy = FT2_START+(lastIndex*3);

        // WRITING IN BOTH FILE TABLES
        fseek(DISK, lastEntryAddress, SEEK_SET);
        fwrite(byte, sizeof(char), 3, DISK);
        fseek(DISK, lastEntryAddressCopy, SEEK_SET);
        fwrite(byte, sizeof(char), 3, DISK);

        u32 fileAddress = (33+entryNumber-2)*512;
        printf("Logic cluster %d\nAddress: %x\n", entryNumber, fileAddress);
        return entryNumber; // Logic block index in file table
}

u32 createFile(char name[8], char extension[3], char attrib, int size){
        unsigned char *cluster_ptr = malloc(512);
        fseek(DISK, ROOTDIR_ADDR, SEEK_SET);
        fread(cluster_ptr, sizeof(char), 512, DISK);
        unsigned char *fileData = malloc(32);
        int ind = 0;
        for(ind = 0; ; ind++){
                if(ind > 13){
                        puts(">>ERRO: sem espaco para novos arquivos");
                        return 0;
                }
                fileData = &cluster_ptr[ind*32];
                if(fileData[0] == 0x00){
                        printf("Arquivo %d: [LIVRE]\n", ind);
                        break;
                }
        }
        printf("%.32s\n", fileData); 

        u16 logicCluster = (u16)allocateFAT();
        printf("Logic cluster: %.2x\n", logicCluster);
        strcpy((char*)fileData, name);
        strcpy((char*)fileData+8, extension);
        fileData[11] = attrib;
        fileData[26] = logicCluster;
        fileData[28] = size;
        printf("%.32s\n", fileData); 
        printf("%.3s\n", &fileData[8]);
        for(int i = 0; i < 32; i++){
                printf("%.2x ", fileData[i]);
                if(i == 15)
                        puts("");
        }
        puts("");
        fseek(DISK, ROOTDIR_ADDR+(32*ind), SEEK_SET);
        fwrite(fileData, sizeof(char), 32, DISK);
        return (33+logicCluster-2)*512;  // Address to the file contents
}

void copyFileToFAT(char fileName[100]){
        FILE* SYSFILE;
        SYSFILE = fopen(fileName, "r");
        if(SYSFILE == NULL){
                puts("ERRO: Arquivo nao encontrado");
                return;
        }
        fseek(SYSFILE, 0, SEEK_END);
        int fileSize = ftell(SYSFILE);
        printf("Filesize: %d bytes\n", fileSize);
        fseek(SYSFILE, 0, SEEK_SET);
        char* buffer = malloc(512);
        fgets(buffer, 511, SYSFILE);

        u32 newFileAddress = createFile(fileName,"", 12, fileSize);
        fseek(DISK, newFileAddress, SEEK_SET);
        fwrite(buffer, sizeof(char), fileSize, DISK);
}  


void handleCopyInput(char input[50]){
        if(!strcmp(input, "1")){
                int fileAddress;
                int fileSize;
                printf("Endereço do arquivo: ");
                scanf("%x", &fileAddress);
                printf("Tamanho do arquivo: ");
                scanf("%x", &fileSize);
                copyFileToSystem(fileAddress, fileSize);
                return;
        }
        if(!strcmp(input, "2")){
                char fileName[100];
                printf("Nome do arquivo: ");
                scanf("%s", fileName);
                copyFileToFAT(fileName);
                return;
        }
        puts("Opção inválida");
        return;
}

void handleInput(char input[50]){
        if(!strcmp(input, "exit")){
                if(DISK != NULL)
                        fclose(DISK);
                exit(0);
        }
        if(!strcmp(input, "mount")){
                char arguments[100] = {};
                scanf("%s", arguments);
                //printf("\n\n%s %s\n", input, arguments);
                if(DISK != NULL){
                        fclose(DISK);
                }
                DISK = fopen(arguments,"rb+");
                if(DISK == NULL){
                        puts(">> ERRO: Arquivo nao encontrado");
                        return;
                }
                //DISK = fopen("./fat12subdir.img","rb+");
                //DISK = fopen("./fat12.img","rb+");
                printf(">> Arquivo %s montado com sucesso\n", arguments);
                return;
        }
        if(DISK == NULL){
                puts(">> ERRO: Nenhum sistema de arquivos montado");
                puts("mount <file> - monta arquivo no programa");
                return;
        }


        if(!strcmp(input, "ls-1")){
                printRootFiles(DISK, 0);
                return;
        }
        if(!strcmp(input, "ls")){
                printRootFiles(DISK, 1);
                return;
        }
        if(!strcmp(input, "cp")){
                puts("Escolha uma opcao:");
                puts("1 - Mover do FAT para o sistema:");
                puts("2 - Mover do sistema para o FAT:");
                puts("Escolha uma opcao:");
                scanf("%s", input);
                handleCopyInput(input);
                return;
        }
        puts("Comando nao encontrado, tente um dos seguintes:");
        puts("mount <file> - monta arquivo no programa");
        puts("ls-1 - mostra apenas diretorios do root");
        puts("ls   - mostra todos diretorios");
        puts("cp   - copia arquivos");
        puts("exit - sai do programa");
}


int main(){
        if(0){
                //DISK = fopen("./fat12subdir.img","rb+");
                DISK = fopen("./fat12.img","rb+");
                //printf("FAT Address 0x%x\n", 512); 
                //printf("FAT Address 0x%x\n", 512+(512*9)); 
                //readFileTable();
                //printFileHex();
                createFile("ARC     ", "C", 12, 100);
                //printFileHex();
                //copyFileToFAT("tofat");
                //printHumanReadableFileTable();
                //printFile(0x4a00, 0x37);
                //copyFileToSystem(0x4a00, 0x37);

                return 0;
        }

        char input[50];
        while(1){
                scanf("%s", input);
                handleInput(input);
        }
        return 0;
}
