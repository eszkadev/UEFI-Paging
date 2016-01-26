/*
Author: Szymon Kłos
14-01-2016
*/

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>
#include "Menu.h"
#include "Paging.h"

/*

--------- < DIRECTORY_START + 1024 ENTRIES * 4 BYTES + 1024 TABLES * 1024 ENTRIES * 4 BYTES
 TABLE  |
 1024   |
---------
  ...   |
---------
TABLE 2 |
---------
TABLE 1 |
--------- < DIRECTORY_START + 1024 ENTRIES * 4 BYTES
	|
 TABLE  |
  DIR	|
	|
--------- < DIRECTORY_START

*/

VOID SetIDTEntry( IN UINT64* address, IN UINT32* pointer, IN UINT8 type, IN UINT16 selector )
{
	UINT16 off1 = (UINT32)pointer;
	UINT16 off2 = (UINT32)pointer >> 16;

	UINT64 entry = *address;
	entry = entry & (~((UINT64)0xffff));
	entry = entry & (~(((UINT64)0xffff)<<48));

	entry |= off1;
	entry |= (UINT64)off2 << 48;

	entry &= ~(((UINT64)0xff) << 32); // zero

	entry &= ~(((UINT64)0xff) << 40); // type
	entry |= (UINT64)type << 40;

	entry &= ~(((UINT64)0xffff) << 16); // selector
	entry |= (UINT64)selector << 16;

	*address = entry;
}

// This function is called from the Page Fault Exception handler
VOID SetPagePresent()
{
	UINT32 cr2 = GetCR2();
	UnicodeSPrint( String, SIZE, L"PAGE FAULT!!! address = %x \n\r", cr2);
	ST->ConOut->OutputString(ST->ConOut, String);

	UINT32* page_directory = (UINT32*)DIRECTORY_START;
	UINT32 PDI = cr2 >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* page = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( cr2 >> 12 ) & 0x03FF;

		page[PTI] |= FLAG_PRESENT;
	}
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
	ST = SystemTable;
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

	UINT32 i;

	UINT32* pointer = 0;
	UINT32* page_directory = (UINT32*)DIRECTORY_START;

	// set each entry to not present
	for(i = 0; i < 1024; i++)
	{
	    page_directory[i] = EMPTY_ENTRY;
	}
 
	pointer = (UINT32*)(DIRECTORY_START + 4096);

	for(i = 0; i < 1024*TABLES; i++)
	{
		pointer[i] = EMPTY_ENTRY;
	}

	// map memory
	UINT32 linear = PAGING_START_ADDR;
	linear = (linear & 0xFFFFF000) - ((( linear >> 12) & 0x03FF)*0x1000);

	pointer = (UINT32*)DIRECTORY_START;
	for(i = 0; i < TABLES; i++)
	{
		pointer += 1024;
		MapAddressIdent(linear, FLAGS, pointer, page_directory);
		linear += 4096*1024;
	}

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Paging x86 \n\r\n\rTurn on paging... ");

	// turn on paging
	SetPageDirectory(page_directory);
	EnablePaging();

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"OK \n\r\n\r");

	// get idt table info
	UINT64 idtr_value = 0;
	UINT8* idtr = (UINT8*)&idtr_value;
	GetIDTR(idtr);

	idtr += 2; // length field
	UINT32 idt = *((UINT32*)idtr);

	UINT32 cs = GetCS();

	// set new page fault handler
	UINT64* idt_entry = (UINT64*)idt + IDT_PAGE_FAULT_OFFSET;

	SetIDTEntry(idt_entry, (UINT32*)PageFaultHandler, IDT_TYPE, cs);

	Menu(SystemTable);

	return EFI_SUCCESS;
}
