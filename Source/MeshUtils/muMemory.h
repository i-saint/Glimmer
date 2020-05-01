#pragma once

void* muMalloc(size_t size, size_t alignment);
void  muFree(void* addr);


// debug API
void muvgInitialize();
bool muvgEnabled();
void muvgReportError();
void muvgPrintRecords();
