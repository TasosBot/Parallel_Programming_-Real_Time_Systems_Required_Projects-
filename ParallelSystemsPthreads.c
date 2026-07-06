#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>

#define NUM_THREADS 8 

typedef struct {
    int thread_id;
    int rowsA;
    int rowsB;
    int cols;
    float* matrixA;
    float* matrixB;
    float* Y_sq;
    float* min1_tot;
    float* min2_tot;
    int* ind1_tot;
    int* ind2_tot;
    pthread_mutex_t* mutex;
} thread_data_t;

void load_matrix(const char* filename, float* matrix, int rows, int cols) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("File opening failed");
        return;
    }
    for (int i = 0; i < rows * cols; i++) {
        if (fscanf(fp, "%f,", &matrix[i]) != 1) break;
    }
    fclose(fp);
}

float* square(float* matrix, int rows, int cols) {
    float* squared = (float*)malloc(rows * sizeof(float));
    if (!squared) return NULL;
    for (int i = 0; i < rows; i++) {
        float sum = 0;
        for (int j = 0; j < cols; j++) {
            sum += matrix[i * cols + j] * matrix[i * cols + j];
        }
        squared[i] = sum;
    }
    return squared;
}

float* product(float* A, float* B, int rowsA, int colsA, int rowsB, int colsB) {
    float* dot = (float*)malloc(rowsA * rowsB * sizeof(float));
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < rowsB; j++) {
            float prod = 0;
            for (int k = 0; k < colsA; k++) {
                prod += A[i * colsA + k] * B[j * colsB + k];
            }
            dot[i * rowsB + j] = prod;
        }
    }
    return dot;
}

void* compute_distances(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int id = data->thread_id;
    int k = 0;

    for (int i = id; i < data->rowsA; i += NUM_THREADS) k++;

    float* local_X = malloc(k * data->cols * sizeof(float));
    
    int row_idx = 0;
    for (int i = id; i < data->rowsA; i += NUM_THREADS) {
        for (int j = 0; j < data->cols; j++) {
            local_X[row_idx * data->cols + j] = data->matrixA[i * data->cols + j];
        }
        row_idx++;
    }

    float* local_X_sq = square(local_X, k, data->cols);
    float* local_prod = product(local_X, data->matrixB, k, data->cols, data->rowsB, data->cols);

    int* ind1 = malloc(data->rowsB * sizeof(int));
    int* ind2 = malloc(data->rowsB * sizeof(int));
    float* d1 = malloc(data->rowsB * sizeof(float));
    float* d2 = malloc(data->rowsB * sizeof(float));

    for (int i = 0; i < data->rowsB; i++) {
        float min1 = INFINITY, min2 = INFINITY;
        int idx1 = -1, idx2 = -1;

        for (int j = 0; j < k; j++) {
            float d_sq = local_X_sq[j] - 2 * local_prod[j * data->rowsB + i] + data->Y_sq[i];
            float d = sqrtf(fmaxf(0.0f, d_sq));
            if (d < min1) {
                min2 = min1; idx2 = idx1;
                min1 = d; idx1 = j;
            } else if (d < min2) {
                min2 = d; idx2 = j;
            }
        }
        ind1[i] = (idx1 != -1) ? (idx1 * NUM_THREADS + id) : -1;
        ind2[i] = (idx2 != -1) ? (idx2 * NUM_THREADS + id) : -1;
        d1[i] = min1;
        d2[i] = min2;
    }

    pthread_mutex_lock(data->mutex);
    for (int i = 0; i < data->rowsB; i++) {
        if (d1[i] < data->min1_tot[i]) {
            data->min2_tot[i] = data->min1_tot[i];
            data->ind2_tot[i] = data->ind1_tot[i];
            data->min1_tot[i] = d1[i];
            data->ind1_tot[i] = ind1[i];
        } else if (d1[i] < data->min2_tot[i]) {
            data->min2_tot[i] = d1[i];
            data->ind2_tot[i] = ind1[i];
        }
        if (d2[i] < data->min2_tot[i]) {
            data->min2_tot[i] = d2[i];
            data->ind2_tot[i] = ind2[i];
        }
    }
    pthread_mutex_unlock(data->mutex);

    free(local_X); free(local_X_sq); free(local_prod);
    free(ind1); free(ind2); free(d1); free(d2);
    pthread_exit(NULL);
}

int main() {
    int rowsA = 10000, rowsB = 20000, cols = 10;
    float* matrixA = malloc(rowsA * cols * sizeof(float));
    float* matrixB = malloc(rowsB * cols * sizeof(float));
    
    load_matrix("matrixAnew.csv", matrixA, rowsA, cols);
    load_matrix("matrixBnew.csv", matrixB, rowsB, cols);

    float* Y_sq = square(matrixB, rowsB, cols);
    float* min1_tot = malloc(rowsB * sizeof(float));
    float* min2_tot = malloc(rowsB * sizeof(float));
    int* ind1_tot = malloc(rowsB * sizeof(int));
    int* ind2_tot = malloc(rowsB * sizeof(int));

    for (int i = 0; i < rowsB; i++) {
        min1_tot[i] = INFINITY; min2_tot[i] = INFINITY;
        ind1_tot[i] = -1; ind2_tot[i] = -1;
    }

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i] = (thread_data_t){i, rowsA, rowsB, cols, matrixA, matrixB, Y_sq, 
                                         min1_tot, min2_tot, ind1_tot, ind2_tot, &mutex};
        pthread_create(&threads[i], NULL, compute_distances, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);

    int choice;
    printf("Enter a Y point index (1-%d): ", rowsB);
    if (scanf("%d", &choice) == 1 && choice >= 0 && choice < rowsB) {
        printf("Indices: 1st: %d, 2nd: %d\n", ind1_tot[choice-1] + 1, ind2_tot[choice-1] + 1);
        printf("Distances: %f, %f\n", min1_tot[choice-1], min2_tot[choice-1]);
    }

    free(matrixA); free(matrixB); free(Y_sq);
    free(min1_tot); free(min2_tot); free(ind1_tot); free(ind2_tot);
    return 0;
}