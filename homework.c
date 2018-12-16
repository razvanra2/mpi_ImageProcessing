#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

const double smoothingFilter[3][3] = {{1.0 / 9, 1.0 / 9, 1.0 / 9},
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
            img->bwData[i] = (unsigned char *) malloc(widthAsNum * sizeof(unsigned char));
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

int main(int argc, char * argv[]) {
    // read input data in image file
    image givenImage;
    readInput(argv[1], &givenImage);
    printf("here\n");
    if (givenImage.type == BW) {  // image is bw
        // init a temp image to work with
        image temp;
        temp.bwData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        for (int i = 0; i < givenImage.height; i++)
        {
            temp.bwData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
        }
        printf("here\n");

        // for each filter
        for (int i = FILTER_START; i < FILTER_END; i++) {
            // copy image data to temp image
            printf("here\n");

            for (int j = 0; j < givenImage.height; j++) {
                memcpy(temp.bwData[j], givenImage.bwData[j], givenImage.width * sizeof(unsigned char));
            }

            // set the filter;
            double filter[3][3];
            if (strcmp(argv[i], "smooth") == 0) {
                memcpy(filter, smoothingFilter, 9 * sizeof(double));
            }
            else if (strcmp(argv[i], "blur") == 0) {
                memcpy(filter, gaussBlurFilter, 9 * sizeof(double));
            }
            else if (strcmp(argv[i], "sharpen") == 0) {
                memcpy(filter, sharpenFilter, 9 * sizeof(double));
            }
            else if (strcmp(argv[i], "mean") == 0) {
                memcpy(filter, meanRemovalFilter, 9 * sizeof(double));
            }
            else if (strcmp(argv[i], "emboss") == 0) {
                memcpy(filter, embossFilter, 9 * sizeof(double));
            }
            printf("here + %s\n", argv[i]);

            // start the threads
            int rank, size;
            MPI_Init(&argc, &argv);
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            MPI_Comm_size(MPI_COMM_WORLD, &size);

            // set the responsability of a thread
            int mulFactor = (int)ceil((1.0 * givenImage.width * givenImage.height) / size);
            int lowBound = mulFactor * rank;
            int highBound = (int)fmin(mulFactor * (rank + 1), givenImage.width * givenImage.height);
            // eaech thread edits the pixels assigned to it
            for (int j = lowBound; j < highBound; j++) {
                int line = j / givenImage.width;
                int column = j % givenImage.width;
                printf("%d %d\n", line, column);

                // do not touch border pixels
                if (line > 1 && column > 1 && line < givenImage.height - 1 && column < givenImage.width - 1) {
                    givenImage.bwData[line][column] = filter[(line) % 3][(column - 1) % 3] * givenImage.bwData[line][column - 1] +
                                               filter[(line) % 3][(column) % 3] * givenImage.bwData[line][column] +
                                               filter[(line) % 3][(column + 1) % 3] * givenImage.bwData[line][column + 1] +
                                               filter[(line - 1) % 3][(column - 1) % 3] * givenImage.bwData[line - 1][column - 1] +
                                               filter[(line - 1) % 3][(column) % 3] * givenImage.bwData[line - 1][column] +
                                               filter[(line - 1) % 3][(column + 1) % 3] * givenImage.bwData[line - 1][column + 1] +
                                               filter[(line + 1) % 3][(column - 1) % 3] * givenImage.bwData[line + 1][column - 1] +
                                               filter[(line + 1) % 3][(column) % 3] * givenImage.bwData[line + 1][column] +
                                               filter[(line + 1) % 3][(column + 1) % 3] * givenImage.bwData[line + 1][column + 1];
                }
            }
            // join the threads
            MPI_Finalize();
        }
        printf("here\n");

        // clean-up temp image data
        for (int i = 0; i < givenImage.height; i++) {
            free(temp.bwData[i]);
        }
        free(temp.bwData);
        printf("here\n");
    } else {  // image is in color
        int rank, size;
        MPI_Init (&argc, &argv);
        MPI_Comm_rank (MPI_COMM_WORLD, &rank);
        MPI_Comm_size (MPI_COMM_WORLD, &size);

        // for each filter
        for (int i = FILTER_START; i < FILTER_END; i++) {
            if (strcmp(argv[i],"smooth") == 0) {

            } else
            if (strcmp(argv[i],"blur") == 0) {

            } else
            if (strcmp(argv[i],"sharpen") == 0) {

            } else
            if (strcmp(argv[i],"mean") == 0) {

            } else
            if (strcmp(argv[i],"emboss") == 0) {

            }
        }
        MPI_Finalize();
    }
    printf("here\n");

    // write the output data (and free image allocated space)
    writeData(argv[2], &givenImage);
    printf("here\n");

    return 0;
}
