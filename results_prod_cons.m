P=[1 2 4 8];
C=[1 2 4 8];
combs=combinations(P,C);
times=[17.56, 33.87, 21.76, 30.81,    56.74, 68.1, 28.01,32.92,    63.62,52.55, 41.44,43.52,  178.41,92.31,69.99,81.58];
combs=table2array(combs);
time_matrix = reshape(times, [4, 4])'; 

h = heatmap({'1','2','4','8'}, {'1','2','4','8'}, time_matrix);
h.Title = 'Producer-Consumer Execution Time (seconds)';
h.XLabel = 'Consumers (C)';
h.YLabel = 'Producers (P)';