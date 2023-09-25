COD InitCod1280_64B R64B_7; // special codelet for setting up data space
LDIMM R64B_1, 0; // Loading base address A
//  LDIMM R64B_2, 1310720; // Loading base address B
//  LDIMM R64B_2, 20480; // 16*16*8*10
LDIMM R64B_2, 81920; // 32*32*8*10
//  LDIMM R64B_3, 2621440; // Loading base address C
//  LDIMM R64B_3, 40960; // 16*16*8*10*2
LDIMM R64B_3, 163840; // 32*32*8*10*2

LDIMM R64B_4, 0; // For i iteration variable
LDIMM R64B_5, 10; // For number of iterations

//  LDIMM R64B_8, 1024; // For offset A
//  LDIMM R64B_9, 131072; // For offset B
//  LDIMM R64B_8, 128;  // 16 * 8
//  LDIMM R64B_9, 2048; // 16 * 16 * 8
LDIMM R64B_8, 256;  // 32 * 8
LDIMM R64B_9, 8192; // 32 * 32 * 8

COD LoadSqTile_2048L R2048L_3, R64B_3, 32;  //  128;  //  16; //Load C
loop:
  BREQ R64B_4, R64B_5, afterLoop;
  ADD R64B_4, R64B_4, 1;
  COD LoadSqTile_2048L R2048L_1, R64B_1, 320;  //  1280; //  160; //Load A
  COD LoadSqTile_2048L R2048L_2, R64B_2, 32;  //  128;  //  16; //Load B
  COD MatMult_2048L R2048L_3, R2048L_1, R2048L_2;
  ADD R64B_1, R64B_1, R64B_8; // *A + 256 (increment by 1 row)
  ADD R64B_2, R64B_2, R64B_9; // *B + 8192 (increment by 1 tile)
  JMPLBL loop;

afterLoop:
COD StoreSqTile_2048L R2048L_3, R64B_3, 32;  //  128;  //  16; //Load C

COMMIT;