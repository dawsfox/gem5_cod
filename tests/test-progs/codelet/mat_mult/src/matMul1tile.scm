COD InitCod_64B R64B_7; // special codelet for setting up data space
LDIMM R64B_1, 0; // Loading base address A
LDIMM R64B_2, 2048;   // 16*16*8
LDIMM R64B_3, 4096;   // 16*16*8*2
LDIMM R64B_4, 0; // For iteration variable
LDIMM R64B_5, 0; // For offset
LDIMM R64B_6, 400; // For number of iterations

COD LoadSqTile_2048L R2048L_1, R64B_1, 16; //Load A
COD LoadSqTile_2048L R2048L_2, R64B_2, 16; //Load B
//  COD LoadSqTile_2048L R2048L_3, R64B_3, 16; //Load C
COD MatMult_2048L R2048L_3, R2048L_1, R2048L_2;
COD StoreSqTile_2048L R2048L_3, R64B_3, 16; //Load C

COMMIT;