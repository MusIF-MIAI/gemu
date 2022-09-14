#include <stdio.h>
#include "msl-commands.h"

#define CC { printf("implement command %s\n", __FUNCTION__); }

void CI01(struct ge* ge) CC
void CI02(struct ge* ge) CC
void CI05(struct ge* ge) CC
void CI06(struct ge* ge) CC
void CI08(struct ge* ge) CC
void CI12(struct ge* ge) CC
void CI19(struct ge* ge) CC
void CI32(struct ge* ge) CC
void CI38(struct ge* ge) CC
void CI39(struct ge* ge) CC
void CI60(struct ge* ge) CC
void CI62(struct ge* ge) CC
void CI65(struct ge* ge) CC
void CI67(struct ge* ge) CC
void CI76(struct ge* ge) CC
void CI80(struct ge* ge) CC
void CI81(struct ge* ge) CC
void CI82(struct ge* ge) CC
void CI83(struct ge* ge) CC
void CI89(struct ge* ge) CC

void CO00(struct ge* ge) CC
void CO02(struct ge* ge) CC
void CO10(struct ge* ge) CC
void CO12(struct ge* ge) CC
void CO30(struct ge* ge) CC
void CO41(struct ge* ge) CC
void CO96(struct ge* ge) CC
void CO97(struct ge* ge) CC

// Set S0
void CU00(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<0)) | (1<<0); }
void CU01(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<1)) | (1<<1); }
void CU02(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<2)) | (1<<2); }
void CU03(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<3)) | (1<<3); }
void CU04(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<4)) | (1<<4); }
void CU05(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<5)) | (1<<5); }
void CU06(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<6)) | (1<<6); }
void CU07(struct ge* ge) { ge->rSO = (ge->rSO & ~(1<<7)) | (1<<7); }

// Reset S0
void CU10(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<0); }
void CU11(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<1); }
void CU12(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<2); }
void CU13(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<3); }
void CU14(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<4); }
void CU15(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<5); }
void CU16(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<6); }
void CU17(struct ge* ge) { ge->rSO = ge->rSO & ~(1<<7); }

void NONE(struct ge* ge) CC
