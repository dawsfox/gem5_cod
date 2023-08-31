COD InitCod_64B R64B_1; // special codelet for setting up data space
LDIMM R64B_1, 0; // Loading base address A
//  LDIMM R64B_2, 131072; // Loading base address B - 128*128*8
//  LDIMM R64B_3, 262144; // Loading base address C - 128*128*8*2
LDIMM R64B_2, 512;   // 8*8*8
LDIMM R64B_3, 1024;  // 8*8*8*2

LDIMM R64B_4, 0; // For iteration variable
LDIMM R64B_5, 0; // For offset
LDIMM R64B_6, 400; // For number of iterations
//  LDIMM R64B_7, 8;

//loop:
//  BREQ R64B_4, R64B_6, 8;
  COD LoadSqTile_2048L R2048L_1, R64B_1, 8; //Load A
  COD LoadSqTile_2048L R2048L_2, R64B_2, 8; //Load B
  COD LoadSqTile_2048L R2048L_3, R64B_3, 8; //Load C
//  COD LoadSqTile_2048L R2048L_1, R64B_1, R64B_7; //Load A
//  COD LoadSqTile_2048L R2048L_2, R64B_2, R64B_7; //Load B
//  COD LoadSqTile_2048L R2048L_3, R64B_3, R64B_7; //Load C
  COD MatMult_2048L R2048L_3, R2048L_1, R2048L_2;
//  COD StoreSqTile_2048L R2048L_3, R64B_3, 128; //Load C
//  COD StoreSqTile_2048L R2048L_3, R64B_3, R64B_7; //Load C
  COD StoreSqTile_2048L R2048L_3, R64B_3, 8; //Load C

//  STOFF R2048L_3, R64B_3, R64B_5;
//  ADD R64B_4, R64B_4, 1;
//  ADD R64B_5, R64B_5, 131072;
//  JMPLBL loop;

COMMIT;