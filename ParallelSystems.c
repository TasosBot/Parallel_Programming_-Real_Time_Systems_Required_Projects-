#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

void load_matrix(const char* filename, float* matrix, int rows, int cols) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("File opening failed");
        return;
    }

    for (int i = 0; i < rows * cols; i++) {
        // %f in fscanf automatically skips spaces, tabs, and newlines
        // If your file has commas, use: fscanf(fp, "%f,", &matrix[i])
        if (fscanf(fp, "%f,", &matrix[i]) != 1) {
            break; 
        }
    }

    fclose(fp);
}
float*  transpose(float* matrix,int rows, int cols){
    float* Tmatrix = (float*)malloc(rows *cols * sizeof(float));
    for (int i=0; i<rows; i++)
{
    for(int j=0; j<cols; j++)
    Tmatrix[j*rows+i]=matrix[i*cols+j];
}
return Tmatrix;}
float* square(float* matrix,int rows,int cols) {
    float* squared=(float*)malloc(rows*sizeof(float));
    if (squared == NULL) return NULL; 
    for (int i=0; i<rows; i++){
        float sum=0;
        for(int j=0; j<cols; j++){
            sum+=matrix[i*cols+j]*matrix[i*cols+j];
        }
    squared[i]=sum;
    }
    return squared;
}
float*  product(float* A,float* B, int rowsA,int colsA, int rowsB, int colsB){
    float* dot=(float*)malloc(rowsA*rowsB*sizeof(float));
//Since, B=Q will not be transposed I need colsA=colsB
if (colsA!=colsB){
    printf("Error\n");
    return NULL;
}
for(int i=0; i<rowsA; i++){
    for(int j=0; j<rowsB; j++){
        float prod=0;
        for (int k=0; k<colsA; k++){
    prod+=A[i*colsA+k]*B[j*colsB+k];
}
dot[i*rowsB+j]=prod;

}

}

return dot;
}
int main() {
  

    
    int rowsA = 10000; 
    int rowsB = 20000; 
    int cols = 10;     
   
    float* matrixA = malloc(rowsA * cols * sizeof(float));
    float* matrixB = malloc(rowsB * cols * sizeof(float));
   
    if (matrixA == NULL || matrixB == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }
    
    load_matrix("matrixAnew.csv", matrixA, rowsA, cols);
    load_matrix("matrixBnew.csv", matrixB, rowsB, cols);
    float* Y_sq=square(matrixB,rowsB,cols);
    float min1_tot[rowsB];
    float min2_tot[rowsB];
    int ind1_tot[rowsB];
    int ind2_tot[rowsB];
    for (int l=0; l<rowsB; l++){
        min1_tot[l]=INFINITY;
        min2_tot[l]=INFINITY;
        ind1_tot[l]=-1;
        ind2_tot[l]=-1;
    }
    #pragma omp parallel 
    {
       int rows_per_thread = (rowsA / omp_get_num_threads()) + 1;
       float* local_X = malloc(rows_per_thread * cols * sizeof(float));
       float* local_d=malloc(rows_per_thread*rowsB*sizeof(float));
   
       
        int id = omp_get_thread_num();
        
      
        int my_row = id ; 
      
        int k=0;
        for (int j=0; j<cols; j++) {
            k=0;
        
        for(int i=id; i<rowsA; i=i+omp_get_num_threads()){
            local_X[k * cols + j]=matrixA[i*cols+j];
            k++;
        }
        
        
        

     //printf("%d",k);
    }
    float* local_X_sq=square(local_X,k,cols);
    
    float* local_prod=product(local_X,matrixB,k,cols,rowsB,cols);
    int* ind1=malloc(rowsB*sizeof(int));
    int* ind2=malloc(rowsB*sizeof(int));
    float* d1=malloc(rowsB*sizeof(float));
    float* d2=malloc(rowsB*sizeof(float));


    for(int i=0; i<rowsB; i++){
        float min1 = INFINITY;
        float min2 = INFINITY;
        int idx1 = -1;
        int idx2 = -1;
        for(int j=0; j<k; j++){
        float d_sq=local_X_sq[j]-2*local_prod[j*rowsB+i]+Y_sq[i]; //1250*20000, will keep best 2,2*20000
        float d = sqrtf(fmaxf(0.0f, d_sq));
            if (d < min1) {
            min2 = min1;
            idx2 = idx1;
            min1 = d;
            idx1 = j;
        } else if (d < min2) {
            min2 = d;
            idx2 = j;
        }
    }
    
    // Store the final best for this row
       ind1[i] = (idx1 != -1) ? (idx1 * omp_get_num_threads() + id) : -1;
        ind2[i] = (idx2 != -1) ? (idx2 * omp_get_num_threads() + id) : -1;
        d1[i] = min1; // i is essentially the y point, we must unify the distances of all threads and find the best 1X20000
        d2[i] = min2;

        }
    
   
    

   #pragma omp critical 
{
    //printf("Thread %d says: Best match for RowB[12] in my chunk is MatrixA index %d\n", id, ind1[12]);
    //printf("Thread %d says: Second Best match for RowB[12] in my chunk is MatrixA index %d\n", id, ind2[12]);

    for (int i=0; i<rowsB; i++){
        if(d1[i]<min1_tot[i]){
            min2_tot[i]=min1_tot[i];
            ind2_tot[i]=ind1_tot[i];
            min1_tot[i]=d1[i];
            ind1_tot[i]=ind1[i];

        }
        else if (d1[i]<min2_tot[i]){
            min2_tot[i]=d1[i];
            ind2_tot[i]=ind1[i];

        }
        if (d2[i]<min2_tot[i]){
            min2_tot[i]=d2[i];
            ind2_tot[i]=ind2[i];
        }


    }



}
 

      free(local_X);
       free(local_prod);
       free(local_d);
       free(local_X_sq);
       free(ind1);
       free(ind2);
       free(d1);
       free(d2);
       
}
 

    free(matrixA);
    free(matrixB);
    int choice;
   printf("Enter a Y point index (1-%d): ", rowsB);
  if (scanf("%d", &choice) == 1 && choice >= 0 && choice < rowsB) {
    printf("Indices of best neighbors are 1st: %d and 2nd: %d\n", ind1_tot[choice-1] + 1, ind2_tot[choice-1] + 1);
    printf("Distances: %f and %f\n", min1_tot[choice-1], min2_tot[choice-1]);
   } else {
    printf("Invalid input.\n");
    }


    return 0;
}
