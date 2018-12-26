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

#define LEADER_RANK 0

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

const float smoothingFilter[9] = {1.0 / 9, 1.0 / 9, 1.0 / 9,
                                    1.0 / 9, 1.0 / 9, 1.0 / 9,
                                    1.0 / 9, 1.0 / 9, 1.0 / 9};

const float gaussBlurFilter[9] = {1.0 / 16, 2.0 / 16, 1.0 / 16,
                                    2.0 / 16, 4.0 / 16, 2.0 / 16,
                                    1.0 / 16, 2.0 / 16, 1.0 / 16};

const float sharpenFilter[9] = {0, -2.0 / 3, 0,
                                    -2.0 / 3, 11.0 / 3, -2.0 / 3,
                                    0, -2.0 / 3, 0};

const float meanRemovalFilter[9] = {-1, -1, -1, -1, 9, -1, -1, -1, -1};
const float embossFilter[9] = {0, 1, 0, 0, 0, 0, 0, -1, 0};

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

    // free allocated memory
    if (img->type == BW) {
        for (int i = 0; i < img->height; i++) {
            free(img->bwData[i]);
        }
        free(img->bwData);
    } else {
        for (int i = 0; i < img->height; i++) {
            free(img->redData[i]);
            free(img->greenData[i]);
            free(img->blueData[i]);

        }
        free(img->redData);
        free(img->greenData);
        free(img->blueData);
    }
    fclose(filePointer);
}

int main(int argc, char * argv[]) {
    // read input data in image file
    image givenImage;
    readInput(argv[1], &givenImage);

    // init a temp image & filter to work with
    image temp;
    image recvImage;
    float filter[9];

    if (givenImage.type == BW) {  // image is bw
        temp.bwData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        recvImage.bwData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        for (int i = 0; i < givenImage.height; i++) {
            temp.bwData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
            recvImage.bwData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
        }
    } else {  // image is in color
        temp.redData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        temp.greenData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        temp.blueData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));

        recvImage.redData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        recvImage.greenData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));
        recvImage.blueData = (unsigned char **)malloc(givenImage.height * sizeof(unsigned char *));

        for (int i = 0; i < givenImage.height; i++) {
            temp.redData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
            temp.greenData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
            temp.blueData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));

            recvImage.redData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
            recvImage.greenData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
            recvImage.blueData[i] = (unsigned char *)malloc(givenImage.width * sizeof(unsigned char));
        }
    }

    // start the threads
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // set the responsability of a thread
    int mulFactor = (int)ceil((1.0 * givenImage.width * givenImage.height) / size);
    int lowBound = mulFactor * rank;
    int highBound = (int)fmin(mulFactor * (rank + 1), givenImage.width * givenImage.height);

    if (givenImage.type == BW) {  // image is bw
        // for each filter
        for (int filterIndex = FILTER_START; filterIndex < FILTER_END; filterIndex++) {
            // set the temp image
            for (int i = 0; i < givenImage.height; i++) {
                memcpy(temp.bwData[i], givenImage.bwData[i],
                givenImage.width * sizeof(unsigned char));
            }

            // set the filter;
            if (strcmp(argv[filterIndex], "smooth") == 0)
            {
                memcpy(filter, smoothingFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "blur") == 0)
            {
                memcpy(filter, gaussBlurFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "sharpen") == 0)
            {
                memcpy(filter, sharpenFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "mean") == 0)
            {
                memcpy(filter, meanRemovalFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "emboss") == 0)
            {
                memcpy(filter, embossFilter, 9 * sizeof(float));
            }
            MPI_Barrier(MPI_COMM_WORLD);
            // apply the filter
            for (int i = lowBound; i < highBound; i++) {
                int line = i / givenImage.width;
                int column = i % givenImage.width;

                // do not touch border pixels
                if (line > 0 && column > 0 && line < givenImage.height - 1
                && column < givenImage.width - 1) {
                    givenImage.bwData[line][column] =
                       filter[0] * temp.bwData[line - 1][column - 1] +
                       filter[1] * temp.bwData[line - 1][column] +
                       filter[2] * temp.bwData[line - 1][column + 1] +
                       filter[3] * temp.bwData[line][column - 1] +
                       filter[4] * temp.bwData[line][column] +
                       filter[5] * temp.bwData[line][column + 1] +
                       filter[6] * temp.bwData[line + 1][column - 1] +
                       filter[7] * temp.bwData[line + 1][column] +
                       filter[8] * temp.bwData[line + 1][column + 1];
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
            // make givenImage with applied filter from all threads
            if (rank != LEADER_RANK) {
                for (int i = 0; i < givenImage.height; i++) {
                    MPI_Send(givenImage.bwData[i], givenImage.width,
                    MPI_UNSIGNED_CHAR, LEADER_RANK, 0, MPI_COMM_WORLD);
                }
            } else {
                for (int i = 0; i < size; i++) {
                    if (i != LEADER_RANK) {
                        for (int j = 0; j < givenImage.height; j++) {
                            MPI_Recv(recvImage.bwData[j],
                                givenImage.width,
                                MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD,
                                MPI_STATUS_IGNORE);
                        }
                        int ilowBound = mulFactor * i;
                        int ihighBound = (int)fmin(mulFactor * (i + 1), givenImage.width * givenImage.height);

                        for (int j = ilowBound; j < ihighBound; j++) {

                            int lineI = j / givenImage.width;
                            int columnI = j % givenImage.width;
                            if (lineI > 0 && columnI > 0 && lineI < givenImage.height - 1
                                && columnI < givenImage.width - 1) {
                                givenImage.bwData[lineI][columnI] = recvImage.bwData[lineI][columnI];
                            }
                        }
                    }
                }
            }

            MPI_Barrier(MPI_COMM_WORLD);

            // send updated givenImage to all processes
            for (int i = 0; i < givenImage.height; i++) {
                MPI_Bcast(givenImage.bwData[i], givenImage.width,
                MPI_UNSIGNED_CHAR, LEADER_RANK, MPI_COMM_WORLD);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    } else {  // image is in color
        // for each filter
        for (int filterIndex = FILTER_START; filterIndex < FILTER_END; filterIndex++) {
            // set the temp image
            for (int i = 0; i < givenImage.height; i++) {
                memcpy(temp.redData[i], givenImage.redData[i],
                givenImage.width * sizeof(unsigned char));

                memcpy(temp.greenData[i], givenImage.greenData[i],
                givenImage.width * sizeof(unsigned char));

                memcpy(temp.blueData[i], givenImage.blueData[i],
                givenImage.width * sizeof(unsigned char));
            }

            // set the filter;
            if (strcmp(argv[filterIndex], "smooth") == 0)
            {
                memcpy(filter, smoothingFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "blur") == 0)
            {
                memcpy(filter, gaussBlurFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "sharpen") == 0)
            {
                memcpy(filter, sharpenFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "mean") == 0)
            {
                memcpy(filter, meanRemovalFilter, 9 * sizeof(float));
            }
            else if (strcmp(argv[filterIndex], "emboss") == 0)
            {
                memcpy(filter, embossFilter, 9 * sizeof(float));
            }
            MPI_Barrier(MPI_COMM_WORLD);
            // apply the filter
            for (int i = lowBound; i < highBound; i++) {
                int line = i / givenImage.width;
                int column = i % givenImage.width;

                // do not touch border pixels
                if (line > 0 && column > 0 && line < givenImage.height - 1
                && column < givenImage.width - 1) {
                    givenImage.redData[line][column] =
                      filter[0] * temp.redData[line - 1][column - 1] +
                      filter[1] * temp.redData[line - 1][column] +
                      filter[2] * temp.redData[line - 1][column + 1] +
                      filter[3] * temp.redData[line][column - 1] +
                      filter[4] * temp.redData[line][column] +
                      filter[5] * temp.redData[line][column + 1] +
                      filter[6] * temp.redData[line + 1][column - 1] +
                      filter[7] * temp.redData[line + 1][column] +
                      filter[8] * temp.redData[line + 1][column + 1];

                    givenImage.greenData[line][column] =
                      filter[0] * temp.greenData[line - 1][column - 1] +
                      filter[1] * temp.greenData[line - 1][column] +
                      filter[2] * temp.greenData[line - 1][column + 1] +
                      filter[3] * temp.greenData[line][column - 1] +
                      filter[4] * temp.greenData[line][column] +
                      filter[5] * temp.greenData[line][column + 1] +
                      filter[6] * temp.greenData[line + 1][column - 1] +
                      filter[7] * temp.greenData[line + 1][column] +
                      filter[8] * temp.greenData[line + 1][column + 1];

                    givenImage.blueData[line][column] =
                     filter[0] * temp.blueData[line - 1][column - 1] +
                     filter[1] * temp.blueData[line - 1][column] +
                     filter[2] * temp.blueData[line - 1][column + 1] +
                     filter[3] * temp.blueData[line][column - 1] +
                     filter[4] * temp.blueData[line][column] +
                     filter[5] * temp.blueData[line][column + 1] +
                     filter[6] * temp.blueData[line + 1][column - 1] +
                     filter[7] * temp.blueData[line + 1][column] +
                     filter[8] * temp.blueData[line + 1][column + 1];
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
            // make givenImage with applied filter from all threads
            if (rank != LEADER_RANK) {
                for (int i = 0; i < givenImage.height; i++) {
                    MPI_Send(givenImage.redData[i], givenImage.width,
                    MPI_UNSIGNED_CHAR, LEADER_RANK, 0, MPI_COMM_WORLD);
                }
            } else {
                for (int i = 0; i < size; i++) {
                    if (i != LEADER_RANK) {
                        for (int j = 0; j < givenImage.height; j++) {
                            MPI_Recv(recvImage.redData[j],
                                givenImage.width,
                                MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD,
                                MPI_STATUS_IGNORE);
                        }
                        int ilowBound = mulFactor * i;
                        int ihighBound = (int)fmin(mulFactor * (i + 1), givenImage.width * givenImage.height);

                        for (int j = ilowBound; j < ihighBound; j++) {

                            int lineI = j / givenImage.width;
                            int columnI = j % givenImage.width;
                            if (lineI > 0 && columnI > 0 && lineI < givenImage.height - 1
                                && columnI < givenImage.width - 1) {
                                givenImage.redData[lineI][columnI] = recvImage.redData[lineI][columnI];
                            }
                        }
                    }
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
            if (rank != LEADER_RANK) {
                for (int i = 0; i < givenImage.height; i++) {
                    MPI_Send(givenImage.greenData[i], givenImage.width,
                    MPI_UNSIGNED_CHAR, LEADER_RANK, 0, MPI_COMM_WORLD);
                }
            } else {
                for (int i = 0; i < size; i++) {
                    if (i != LEADER_RANK) {
                        for (int j = 0; j < givenImage.height; j++) {
                            MPI_Recv(recvImage.greenData[j],
                                givenImage.width,
                                MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD,
                                MPI_STATUS_IGNORE);
                        }
                        int ilowBound = mulFactor * i;
                        int ihighBound = (int)fmin(mulFactor * (i + 1), givenImage.width * givenImage.height);

                        for (int j = ilowBound; j < ihighBound; j++) {

                            int lineI = j / givenImage.width;
                            int columnI = j % givenImage.width;
                            if (lineI > 0 && columnI > 0 && lineI < givenImage.height - 1
                                && columnI < givenImage.width - 1) {
                                givenImage.greenData[lineI][columnI] = recvImage.greenData[lineI][columnI];
                            }
                        }
                    }
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
            if (rank != LEADER_RANK) {
                for (int i = 0; i < givenImage.height; i++) {
                    MPI_Send(givenImage.blueData[i], givenImage.width,
                    MPI_UNSIGNED_CHAR, LEADER_RANK, 0, MPI_COMM_WORLD);
                }
            } else {
                for (int i = 0; i < size; i++) {
                    if (i != LEADER_RANK) {
                        for (int j = 0; j < givenImage.height; j++) {
                            MPI_Recv(recvImage.blueData[j],
                                givenImage.width,
                                MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD,
                                MPI_STATUS_IGNORE);
                        }
                        int ilowBound = mulFactor * i;
                        int ihighBound = (int)fmin(mulFactor * (i + 1), givenImage.width * givenImage.height);

                        for (int j = ilowBound; j < ihighBound; j++) {

                            int lineI = j / givenImage.width;
                            int columnI = j % givenImage.width;
                            if (lineI > 0 && columnI > 0 && lineI < givenImage.height - 1
                                && columnI < givenImage.width - 1) {
                                givenImage.blueData[lineI][columnI] = recvImage.blueData[lineI][columnI];
                            }
                        }
                    }
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);
            // send updated givenImage to all processes
            for (int i = 0; i < givenImage.height; i++) {
                MPI_Bcast(givenImage.redData[i], givenImage.width,
                MPI_UNSIGNED_CHAR, LEADER_RANK, MPI_COMM_WORLD);

                MPI_Bcast(givenImage.greenData[i], givenImage.width,
                MPI_UNSIGNED_CHAR, LEADER_RANK, MPI_COMM_WORLD);

                MPI_Bcast(givenImage.blueData[i], givenImage.width,
                MPI_UNSIGNED_CHAR, LEADER_RANK, MPI_COMM_WORLD);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    // join the threads
    MPI_Finalize();

    // clear up temp image data
    if (givenImage.type == BW) {  // image is bw
        for (int i = 0; i < givenImage.height; i++) {
            free(temp.bwData[i]);
            free(recvImage.bwData[i]);
        }
        free(temp.bwData);
        free(recvImage.bwData);
    } else {
        for (int i = 0; i < givenImage.height; i++) {
            free(temp.redData[i]);
            free(temp.greenData[i]);
            free(temp.blueData[i]);

            free(recvImage.redData[i]);
            free(recvImage.greenData[i]);
            free(recvImage.blueData[i]);
        }
        free(temp.redData);
        free(temp.greenData);
        free(temp.blueData);

        free(recvImage.redData);
        free(recvImage.greenData);
        free(recvImage.blueData);
    }

    // write the output data (and free image allocated space)
    writeData(argv[2], &givenImage);

    return 0;
}
