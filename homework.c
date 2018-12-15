#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE_BW 1
#define IMAGE_SIZE_COL 3
#define BW 5
#define COLOR 6
#define UNDEFINED -1

#define FILTER_START 3
#define FILTER_END argc

typedef struct {
    int type;  // store all data for any type of image
    int width;
    int height;
    int maxval;
    unsigned char** bwData;  // bw image case
    unsigned char** redData;  // color image case
    unsigned char** greenData;
    unsigned char** blueData;
}image;

const double smoothingFilter[3][3] ={{1.0 / 9, 1.0 / 9, 1.0 / 9},
                                    {1.0 / 9, 1.0 / 9, 1.0 / 9},
                                    {1.0 / 9, 1.0 / 9, 1.0 / 9}};

const double gaussBlurFilter[3][3] = {{1.0 / 16, 2.0 / 16, 1.0 / 16},
                                    {2.0 / 16, 4.0 / 16, 2.0 / 16},
                                    {1.0 / 16, 2.0 / 16, 1.0 / 16}};

const double sharpenFilter[3][3] = {{0, -2.0 / 3, 0},
                                    {- 2.0 / 3, 11.0 / 3, -2.0 / 3},
                                    {0, -2.0 / 3, 0}};

const double meanRemovalFilter[3][3] = {{0, 0, 0}, {0, 1, 0}, {0, 0, 0}};

const double embossFilter[3][3] = {{0, 1, 0}, {0, 0, 0}, {0, -1, 0}};

void readInput(const char * fileName, image *img) {
    // open input file for reading
    FILE *filePointer = fopen(fileName, "rb");
    if (filePointer == NULL) {
        perror("error opening input file");
        exit(1);
    }

    // read image type in tempImageTypeBuffer
    char tempImageType[3] = {'\0'};
    fread(tempImageType, 1, 2,filePointer);
    // read \n in junk
    char junk[1];
    fread(junk, 1, 1, filePointer);

    // read image width in width buffer, then convert to integer widthAsNum
    char width[10] = {'\0'};
    int widthAsNum = -1;
    int i = 0;
    fread(junk, 1, 1, filePointer);
    while(junk[0] != ' ') {
        width[i] = junk[0];
        i++;
        fread(junk, 1, 1, filePointer);
    }
    widthAsNum = atoi(width);

    // read image height in height buffer, then convert to integer heightAsNum
    char height[10] = {'\0'};
    int heightAsNum = -1;
    i = 0;
    fread(junk, 1, 1, filePointer);
    while(junk[0] != '\n') {
        height[i] = junk[0];
        i++;
        fread(junk, 1, 1, filePointer);
    }
    heightAsNum = atoi(height);

    //read maxval in maxval buffer, then convert to integer maxvalAsNum
    char maxval[10] = {'\0'};
    int maxvalAsNum = -1;
    i = 0;
    fread(junk, 1, 1, filePointer);
    while(junk[0] != '\n') {
        maxval[i] = junk[0];
        i++;
        fread(junk, 1, 1, filePointer);
    }
    maxvalAsNum = atoi(maxval);

    // read image data
    if (tempImageType[1] == '5') {  // image is b&w
        img->bwData = (unsigned char **) malloc(heightAsNum * sizeof(unsigned char *));
        for (int i = 0; i < heightAsNum; i++) {
            img->bwData[i] = (unsigned char *) malloc(widthAsNum * sizeof(unsigned char *));
        }

        unsigned char *buffer = (unsigned char *)malloc(widthAsNum * heightAsNum);
    	fread(buffer, sizeof(unsigned char), widthAsNum * heightAsNum, filePointer);

    	int k = 0;
    	for (int i = 0; i < heightAsNum; i++) {
    		for (int j = 0; j < widthAsNum; j++) {
    			img->bwData[i][j] = buffer[k++];
    		}
    	}
    	free(buffer);
        img->redData = NULL;
        img->greenData = NULL;
        img->blueData = NULL;
    } else {  // image is in color
        img->redData = (unsigned char **) malloc(heightAsNum * sizeof(unsigned char *));
        img->greenData = (unsigned char **) malloc(heightAsNum * sizeof(unsigned char *));
        img->blueData = (unsigned char **) malloc(heightAsNum * sizeof(unsigned char *));

        for (int i = 0; i < heightAsNum; i++) {
            img->redData[i] = (unsigned char *)malloc(widthAsNum * sizeof(unsigned char));
            img->greenData[i] = (unsigned char *)malloc(widthAsNum * sizeof(unsigned char));
            img->blueData[i] = (unsigned char *)malloc(widthAsNum * sizeof(unsigned char));
        }

        unsigned char *buffer = (unsigned char *)malloc(heightAsNum * widthAsNum * 3);
        fread(buffer, 1, heightAsNum * widthAsNum * 3, filePointer);

        int k = 0;
        for (int i = 0; i < heightAsNum; i++) {
            for (int j = 0; j < widthAsNum; j++) {
                img->redData[i][j] = buffer[k++];
                img->greenData[i][j] = buffer[k++];
                img->blueData[i][j] = buffer[k++];
            }
        }
        free(buffer);
        img->bwData = NULL;
    }
    // close input file
    fclose(filePointer);

    // put read image metadata inside the actual data structure
    if (tempImageType[1] == '5') {
        img->type = BW;
    } else {
        img->type = COLOR;
    }
    img->width = widthAsNum;
    img->height = heightAsNum;
    img->maxval = maxvalAsNum;
}

void writeData(const char * fileName, image *img) {
    // open output file for writing
    FILE *filePointer;
    filePointer = fopen(fileName, "wb");
    if (filePointer == NULL) {
        perror("error opening input file");
        exit(1);
    }

    if (img->type == BW)
    {
        char imgTypeLine[] = "P5\n";
        fwrite(&imgTypeLine, sizeof(char), 3 * sizeof(char), filePointer);
    } else {
        char imgTypeLine[] = "P6\n";
        fwrite(&imgTypeLine, sizeof(char), 3 * sizeof(char), filePointer);
    }

    //write image width and height line
    char widthArray[100];
    char heightArray[100];
    sprintf(widthArray, "%d", img->width);
    sprintf(heightArray, "%d", img->height);
    char whiteSpace[] = " ";
    char newLine[] = "\n";
    fwrite(&widthArray, sizeof(char), strlen(widthArray), filePointer);
    fwrite(&whiteSpace, sizeof(char), 1 * sizeof(char), filePointer);
    fwrite(&heightArray, sizeof(char), strlen(heightArray), filePointer);
    fwrite(&newLine, sizeof(char), 1 * sizeof(char), filePointer);
    // write maxval line
    char maxvalArray[100];
    sprintf(maxvalArray, "%d", img->maxval);
    fwrite(&maxvalArray, sizeof(char), strlen(maxvalArray), filePointer);
    fwrite(&newLine, sizeof(char), 1 * sizeof(char), filePointer);

    // write image data
    if (img->type == BW || img->type == COLOR) {
        if (img->type == BW) {
            for (int i = 0; i < img->height; i++) {
                fwrite(img->bwData[i], sizeof(unsigned char), img->width, filePointer);
            }
        } else {
            for (int i = 0; i < img->height; i++) {
                int k = 0;
                for (int j = 0; j < img->width * 3; j++) {
                    if (j % 3 == 0) {
                        fwrite(&img->redData[i][k], 1, sizeof(unsigned char), filePointer);
                    }
                    if (j % 3 == 1) {
                        fwrite(&img->greenData[i][k], 1, sizeof(unsigned char), filePointer);
                    }
                    if (j % 3 == 2) {
                        fwrite(&img->blueData[i][k], 1, sizeof(unsigned char), filePointer);
                        k++;
                    }
                }
            }
        }
    }
    // safety check
    // do not free unallocated memory
    if (img->type == BW || img->type == COLOR) {
        if (img->type == BW) {
            for (int i = 0; i < img->height; i++) {
                free(img->bwData[i]);
            }
            free(img->bwData);
        }
    }
    fclose(filePointer);
}

void applyFiltersBW(image *img, char * filterNames[]) {

}

void applyFiltersColor(image *img, char * filterNames[]) {

}


int main(int argc, char * argv[]) {
    // read input data in image file
    image image;
    readInput(argv[1], &image);

    int rank, size;
    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    // for each filter
    for (int i = FILTER_START; i < FILTER_END; i++) {

    }

    MPI_Finalize();
    // write the output data (and free image allocated space)
    writeData(argv[2], &image);

    return 0;
}
