/*
Author: Szymon Kłos
14-01-2016
*/

#ifndef _PAGING_H_
#define _PAGING_H_

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>
#include "Paging.h"

#define DIRECTORY_START 0x4020000
#define TABLES 1024
#define PAGING_START_ADDR 0

#define IDT_TYPE 0x8E
#define IDT_PAGE_FAULT_OFFSET 0x0E

// RW, SUPERVISOR, NOT PRESENT
#define EMPTY_ENTRY 2
// RW, SUPERVISOR, PRESENT
#define FLAGS 3

#define FLAG_PRESENT 1

extern void SetPageDirectory(UINT32*);
extern void EnablePaging();
extern void GetIDTR(void*);
extern UINT32 GetCR2();
extern UINT32 GetCS();
extern void PageFaultHandler();

EFI_SYSTEM_TABLE* ST;

VOID MapAddress( IN UINT32 linear, IN UINT32 phys, IN UINT16 flags, IN UINT32* page_directory );
VOID MapAddressIdent( IN UINT32 address, IN UINT16 flags, IN UINT32* table, IN UINT32* page_directory );
VOID SetNotPresent( IN UINT32 linear, IN UINT32* page_directory );
VOID SetPagePresent();

#endif
