/*
Author: Szymon Kłos
14-01-2016
*/

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>
#include "Paging.h"

VOID MapAddress( IN UINT32 linear, IN UINT32 phys, IN UINT16 flags, IN UINT32* page_directory )
{
	UINT32 PDI = linear >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* table = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( linear >> 12 ) & 0x03FF;
		table[PTI] = phys | flags;
	}
}

VOID MapAddressIdent( IN UINT32 address, IN UINT16 flags, IN UINT32* table, IN UINT32* page_directory )
{
	UINT32 PDI = address >> 22;

	if(page_directory[PDI] == EMPTY_ENTRY)
	{
		page_directory[PDI] = (UINT32)(table) | flags;

		UINT32 i;
		for(i = 0; i < 1024; i++)
		{
			table[i] = address | flags;
			address = address + 4096;
		}
	}
}

VOID SetNotPresent( IN UINT32 linear, IN UINT32* page_directory )
{
	UINT32 PDI = linear >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* page = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( linear >> 12 ) & 0x03FF;

		page[PTI] &= ~((UINT32)FLAG_PRESENT);
	}
}

